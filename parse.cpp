#include <iterator>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <list>
#include "cvs.h"
#include "deg.h"

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
   double stop_lat,stop_lon; // Latitude, Longitude

   int nb_hops;      // number of stops crossed to get there (stops, not train changes)
   int best_time;    // in minutes
   Source *best_source;
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
   int travel_time;  // total travel time from the source, not just 1 hop
   int arrival_time; // we arrive at arrival_time (minutes)
   int walking;      // proper connection or walking?
   Source *best;     // best possible route to here
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
   double stop_lat, stop_lon;
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

// int (minutes) -> xx:xx
int hour(int v) { return v/60; }
int minutes(int v) { return v%60; }
string h(int v) { return _ + to_string(hour(v)) + ":" + to_string(minutes(v)); }


void init_vertices(void) {
   vertices = (struct vertex*)calloc(nb_stops, sizeof(*vertices));
}

void add_edge(int v, int dst, int travel_time, int departure_time) {
   int index = vertices[v].nb_edges;
   for(int i = 0; i < index; i++) {
      if(vertices[v].edges[i].dst == dst
            && vertices[v].edges[i].departure_time == departure_time) {
         // Update the travel time if for some reason two trains depart at the same time to the same destination but one is faster
         if(vertices[v].edges[i].travel_time > travel_time)
            vertices[v].edges[i].travel_time = travel_time;
         // Ignore edge, it is already there!
         return;
      }
   }
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
         add_edge(parent->id, current->id, travel_time, previous_time);
      }

      parent = current;
      previous_time = string_to_time(departure_time);

      nb_trips++;
      if(nb_trips % 300000 == 0)
         cout << nb_trips << "/ 11259226 = " << (nb_trips*100/11259226) << "%\n";
   }
}

void create_transfers(char *dir) {
   string trip_file_name = _ + dir + "/transfers.txt";
   io::CSVReader<3, trim_chars<' ', '\t'>, double_quote_escape<',','\"'> > in(trip_file_name);
   in.read_header(io::ignore_extra_column, "from_stop_id", "to_stop_id", "min_transfer_time");

   string from_stop_id, to_stop_id;
   int min_transfer_time;

   while(in.read_row(from_stop_id, to_stop_id, min_transfer_time)) {
      Stop *parent = stops[from_stop_id];
      Stop *dst = stops[to_stop_id];
      if(parent == dst)
         continue;
      add_edge(parent->id, dst->id, min_transfer_time/60, -1);
   }
}

void create_walks(void) {
   for (auto it1 = stop_names.begin(); it1 != stop_names.end(); ++it1) {
      for (auto it2 = stop_names.begin(); it2 != stop_names.end(); ++it2) {
         Stop *s1 = it1->second;
         Stop *s2 = it2->second;
         if(s1 == s2)
            continue;
         double dst = distanceEarth(s1->stop_lat, s1->stop_lon, s2->stop_lat, s2->stop_lon);
         if(dst < 100) { // less than 100m
                         // add a walk path!
            //cout << "Adding edge between " << s1->stop_name << " and " << s2->stop_name << "\n";
            add_edge(s1->id, s2->id, 2, -1);
         }
      }
   }
}

int _add_connection(Stop *dst, Source *f) {
   for (auto it = dst->parents.begin(); it != dst->parents.end(); ++it) {
      Source *e = *it;
      if(e->arrival_time == f->arrival_time) {
         if(e->travel_time <= f->travel_time) {
            // ignore f
         } else {
            // found a shorter way that arrives at the same time!
            e->travel_time = f->travel_time;
            e->parent = f->parent;
            dst->next = 1;
         }
         return 1;
      }
   }
   dst->parents.push_back(f);
   dst->next = 1;
   return 0;
}

/* Add a connection to a vertex -- dst can be reached from the Source f */
void add_connection(Stop *dst, Source *f, int iteration) {
   // Did we find the best time?
   if(dst->best_time == -1 || dst->best_time > f->travel_time) {
      dst->best_time = f->travel_time;
      dst->nb_hops = iteration;
      dst->best_source = f;
   }

   // Add the trajectory to the list of possible trajectories if it is close the the best (less than 60 minutes worse than best)
   if(f->travel_time < dst->best_time + 60 && f->travel_time < dst->best_time * 2) {
      /*cout << "Found connection " << src->stop_name << " -> " << dst->stop_name << " at " << h(e->departure_time)
        << " best " << h(dst->best_time) << " ours " << h(f->travel_time)
        << " departure at " << h(best->arrival_time) << " takes " << h(f->travel_time) << " because edge travel is " << h(e->travel_time)
        << " so far " << dst->parents.size() << " trajectories " << "\n";*/
      int already_there = _add_connection(dst, f);
      if(already_there && dst->best_source != f)
         delete f;
   } else {
      /*cout << "Ignoring connection " << src->stop_name << " -> " << dst->stop_name << " at " << h(e->departure_time)
        << " best " << h(dst->best_time) << " ours " << h(f->travel_time)
        << " so far " << dst->parents.size() << " trajectories " << "\n";*/
      assert(dst->best_source != f);
      delete f;
   }
}

/* Get all the trajectories leaving src, and check if it adds new possibilities to the destinations reachable from src */
void __sssp(Stop *src, int iteration) {
   struct edge *edges = vertices[src->id].edges;
   for(size_t i = 0; i < vertices[src->id].nb_edges; i++) {
      struct edge *e = &edges[i];
      Stop *dst = stop_ids[e->dst];
      if(!dst) {
         cout << "Bug\n";
         continue;
      }

      Source *f = NULL;
      if(e->departure_time == -1) { // we can use that edge whenever (foot/bike transfer)
         // So push the edge for all transfers that end up in src
         for (auto it = src->parents.begin(); it != src->parents.end(); ++it) {
            Source *parent = *it;
            if(parent->parent == dst) // don't loop stupidly! Might happen because of walking back and forth between station and bus stop!
               continue;
            if(parent->walking) // don't walk twice, be lazy (avoid combinatory explosion)
               continue;
            f = new Source();
            f->parent = src;
            f->arrival_time = parent->arrival_time + e->travel_time;
            f->travel_time = parent->travel_time + e->travel_time;
            f->best = parent;
            f->walking = 1;
            add_connection(dst, f, iteration);
         }
      } else {
         Source *best = NULL;
         int best_time = -1;
         // What's the best previous connection that allows us to use that train?
         for (auto it = src->parents.begin(); it != src->parents.end(); ++it) {
            Source *parent = *it;
            // If we are the source of the sssp, then we are always best
            if(parent->parent == NULL) { // source is the only node without parent!
               best = parent;
               best_time = 0;
               break;
            }

            // Ignore impossible connections
            if(e->departure_time < parent->arrival_time)
               continue;

            // Time to reach destination is time already spent traveling + waiting time
            int time = parent->travel_time + (e->departure_time - parent->arrival_time);
            if(time < best_time || best_time == -1) {
               best_time = time;
               best = parent;
            }
         }

         // It is possible that we cannot use that connection (too early to be reachable)
         if(!best)
            continue;

         // Otherwise we found a way
         f = new Source();
         f->parent = src;
         f->arrival_time = e->departure_time + e->travel_time;
         f->travel_time = best_time + e->travel_time;
         f->best = best;
         f->walking = 0;
         add_connection(dst, f, iteration);
      }
   }
}

// Do the  __sssp for all active vertices
void _sssp(int iterations) {
   cout << "Iteration " << iterations << "\n";

   for (auto it = stop_names.begin(); it != stop_names.end(); ++it) {
      Stop *s = it->second;
      if(s->active) {
         __sssp(s, iterations);
      }
   }

   int nb_active = 0;
   for (auto it = stop_names.begin(); it != stop_names.end(); ++it) {
      Stop *s = it->second;
      s->active = s->next;
      s->next = 0;
      if(s->active)
         nb_active++;
   }
   if(nb_active && iterations < 200) // and recurse until we don't have any active vertex anymore
      _sssp(iterations + 1);
}

int sssp(Stop *src) {
   /* Best way to get to source is empty route */
   Source *s = new Source();
   s->parent = NULL;
   s->travel_time = 0;
   s->arrival_time = 0;
   s->best = NULL;
   s->walking = 0;
   src->parents.push_back(s);

   /* Initialize stats */
   src->nb_hops = 0;
   src->best_time = 0;
   src->best_source = NULL;
   src->active = 1;

   /* Run */
   _sssp(0);

   /* Report best times to get to all other stops */
   std::vector<std::pair<int,Stop*>> dst;
   for (auto it = stop_names.begin(); it != stop_names.end(); ++it) {
      Stop *s = it->second;
      dst.push_back({s->best_time, s});
   }
   std::sort(dst.begin(), dst.end()); // sort by time
   for (auto it = dst.begin(); it != dst.end(); ++it) {
      Stop *s = it->second;
      if(s->best_time > 0)
         cout << s->stop_name << " in " << s->best_time << " minutes\n";
   }
}

void best_path(string d) {
   cout << "-----\n";
   Stop *dst = stop_names[d];
   Source *f = dst->best_source;
   while(f) {
      cout << "Arrival in " << dst->stop_name << " at " << h(f->arrival_time) << " in total " << h(f->travel_time) << "\n";
      dst = f->parent;
      f = f->best;
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

   // Parse transferts.txt
   create_transfers(argv[1]);

   // Connect all stations that are close enough to be walkable
   create_walks();

   // Find shortest travel times from origin point.
   sssp(origin);

   //Test
   best_path("Martigny");
   best_path("Martigny, gare");
}
