#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <list>
#include "cvs.h"

using namespace std;
using namespace io;
string _ = "";

/* Previous stop in a trajectory */
struct Source;

/* Stop (train station, bus stop, ...) */
struct Stop {
   int id;           // our internal id, not stop_id
   int active, next; // active in current iteration of sssp, in next iteration
   string stop_name; // E.g. "Lausanne"
   float stop_lat,stop_lon; // Latitude, Longitude

   int nb_hops;      // number of stops crossed to get there (stops, not train changes)
   int best_time;    // in minutes
   list<Source*> parents; // different ways and times to get there; we chose the best one for every given bus/train leaving from us
                          // the best depends on travel time up until here, and arrival time
};
int nb_stops = 0;
std::map<string,Stop*> stops;       // stop_id (GTFS) => Stop
std::map<string,Stop*> stop_names;  // stop_name (GTFS) => Stop
std::map<int,Stop*> stop_ids;       // our internale id => Stop

/* Different ways to get to a Stop */
struct Source {
   Stop *parent;
   int travel_time;
   int arrival_time;
};

/* Graph representation for shortest path computation */
struct edge {
   int dst;
   int travel_time;
   int departure_time;
};
struct vertex {
   int nb_edges;
   struct edge *edges;
};
struct vertex *vertices;


/* stops.txt => Stop objects */
Stop* create_stops(char *dir, string origin) {
   Stop *ret;

   string stop_file_name = _ + dir + "/stops.txt";
   io::CSVReader<4, trim_chars<' ', '\t'>, double_quote_escape<',','\"'> > in(stop_file_name);
   in.read_header(io::ignore_extra_column, "stop_id", "stop_name", "stop_lat", "stop_lon");

   string stop_id, stop_name;
   float stop_lat, stop_lon;
   while(in.read_row(stop_id, stop_name, stop_lat, stop_lon)) {
      Stop *s;
      // Multiple stop_id are used for the same station... actually one per track...
      // We merge them because we don't really care about that.
      if(stop_names[stop_name]) {
         s = stop_names[stop_name];
      } else {
         s = new Stop();
         s->stop_name = stop_name;
         s->id = nb_stops;
         s->stop_lat = stop_lat;
         s->stop_lon = stop_lon;
         s->active = 0;
         s->next = 0;
         s->nb_hops = -1;
         s->best_time = -1;
         stop_ids[s->id] = s;
         stop_names[stop_name] = s;
         nb_stops++;
      }
      stops[stop_id] = s;

      if(stop_name == origin)
         ret = s;
   }
   return ret;
}

int char_to_int(char v) {
   return v - '0';
}
// 05:15:00 -> int (in minutes)
int string_to_time(string time) {
   return (char_to_int(time[0])*10 + char_to_int(time[1])) * 60
      + (char_to_int(time[3])*10 + char_to_int(time[4]));
}

void init_vertices(void) {
   vertices = (struct vertex*)calloc(nb_stops, sizeof(*vertices));
}

void add_edge(int v, int dst, int travel_time, int departure_time) {
   int index = vertices[v].nb_edges;
   vertices[v].nb_edges++;
   vertices[v].edges = (struct edge*)realloc(vertices[v].edges, vertices[v].nb_edges*sizeof(*vertices[v].edges));
   vertices[v].edges[index].dst = dst;
   vertices[v].edges[index].travel_time = travel_time;
   vertices[v].edges[index].departure_time = departure_time;
}

/* Build the graph of all trajectories */
void create_trips(char *dir) {
   string trip_file_name = _ + dir + "/stop_times.txt";
   io::CSVReader<5, trim_chars<' ', '\t'>, double_quote_escape<',','\"'> > in(trip_file_name);
   in.read_header(io::ignore_extra_column, "trip_id", "arrival_time", "departure_time", "stop_id","stop_sequence");

   string trip_id, arrival_time, departure_time, stop_id;
   int stop_sequence, nb_trips = 0;

   Stop *parent;
   int previous_time;
   while(in.read_row(trip_id, arrival_time, departure_time, stop_id, stop_sequence)) {
      Stop *current = stops[stop_id];
      if(!current) {
         cout << "Unknown stop " << stop_id << "\n";
         continue;
      }

      if(stop_sequence != 1 && parent) {
         int current_time = string_to_time(arrival_time);
         int travel_time = current_time - previous_time;
         if(travel_time < 0) {
            cout << previous_time << " " << current_time << "\n";
            continue;
         }
         add_edge(parent->id, current->id, travel_time, current_time);
      }

      parent = current;
      previous_time = string_to_time(departure_time);

      nb_trips++;
      if(nb_trips % 300000 == 0)
         cout << nb_trips << "/ 11259226 = " << (nb_trips*100/11259226) << "%\n";
   }
}

/* Get all the trajectories leaving srv, and check if it adds new possibilities to the destinations reachable from src */
void __sssp(Stop *src, int iteration) {
   struct edge *edges = vertices[src->id].edges;
   for(size_t i = 0; i < vertices[src->id].nb_edges; i++) {
      struct edge *e = &edges[i];
      Stop *dst = stop_ids[e->dst];
      if(!dst) {
         cout << "Bug\n";
         continue;
      }

      // What's the best previous connexion that gets us to that point?
      Source *best = NULL;
      int best_time = -1;
      for (auto it = src->parents.begin(); it != src->parents.end(); ++it) {
         Source *parent = *it;
         // We are the source (no parent) so we are always the best choice
         if(parent->parent == NULL) {
            best = parent;
            best_time = 0;
            break;
         }

         // Ignore impossible connexions
         if(e->departure_time < parent->arrival_time)
            break;

         // Time to reach destination is time already spent traveling + waiting time
         int time = parent->travel_time + (e->departure_time - parent->arrival_time);
         if(time < best_time || best_time == -1) {
            best_time = time;
            best = parent;
         }
      }

      if(!best) {
         //cout << "We cannot use a connexion because it is impossible to get there in time...\n";
         continue;
      }

      Source *f = new Source();
      f->parent = src;
      f->arrival_time = e->departure_time + e->travel_time;
      f->travel_time = best_time + e->travel_time;

      // Did we find the best time?
      if(dst->best_time == -1 || dst->best_time > f->travel_time) {
         dst->best_time = f->travel_time;
         dst->nb_hops = iteration;
      }

      // Add the trajectory to the list of possible trajectories if it is close the the best (less than 60 minutes worse than best)
      if(f->travel_time < dst->best_time + 60 && f->travel_time < dst->best_time * 2) {
         dst->parents.push_back(f);
         dst->active = 1;
      }
   }
}

// Do the  __sssp for all active vertices
void _sssp(int iterations) {
   cout << "Iteration " << iterations << "\n";

   for (auto it = stops.begin(); it != stops.end(); ++it) {
      Stop *s = it->second;
      if(s->active) {
         __sssp(s, iterations);
      }
   }

   int nb_active = 0;
   for (auto it = stops.begin(); it != stops.end(); ++it) {
      Stop *s = it->second;
      s->active = s->next;
      s->next = 0;
      if(s->active)
         nb_active++;
   }
   if(nb_active && iterations < 100) // and recurse until we don't have any active vertex anymore
      _sssp(iterations + 1);
}

int sssp(Stop *source) {
   Source *s = new Source();
   s->parent = NULL;
   s->travel_time = 0;
   s->arrival_time = 0;
   source->parents.push_back(s);
   source->nb_hops = 0;
   source->best_time = 0;
   source->active = 1;
   _sssp(0);

   for (auto it = stop_names.begin(); it != stop_names.end(); ++it) {
      Stop *s = it->second;
      if(s->best_time != -1) {
         cout << s->stop_name << " in " << s->best_time << " minutes\n";
      }
   }
}

int main(int argc, char **argv) {
   if(argc < 2) {
      cout << "Usage: parse <directory containing gtfs data> <stop name>\n";
      cout << "\tWill compute the shortest travel time from <stop name> to all other stops in the GTFS dataset\n";
      cout << "\tE.g.: ./parse . Lausanne\n";
      return -1;
   }

   // Parse stops.txt and return the Stop object corresponding to <stop name>
   Stop *origin = create_stops(argv[1], argv[2]);
   if(!origin) {
      cout << "Origin stop not found\n";
      return -1;
   }
   cout << "#Pathes from " << origin->stop_name << " uid " << origin->id << "\n";

   // Convert stops into a vertex array
   init_vertices();

   // Parse stop_times.txt
   create_trips(argv[1]);

   // Find shortest travel times from origin point.
   sssp(origin);
}
