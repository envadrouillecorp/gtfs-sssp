# gtfs-sssp

Compute the shortest path from a given train station to all other train stations in a GTFS data set.

## Usage

./parse "directory where GTFS files are" "train station"

E.g.: ./parse . Lausanne

## Output

The output will be a json file that can be used in map.html to create maps that look like that:

![Lausanne](https://raw.githubusercontent.com/envadrouillecorp/gtfs-sssp/master/lausanne01.jpg)

## Notes

* Very inefficient code. It takes ~6 minutes on a powerful desktop to create the json file with Switzerland GTFS data. It does the job though...

* The code automatically adds walk paths between train stations and nearby bus stops (and vice versa).
