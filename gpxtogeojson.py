import dateutil.parser
import fiona
import glob
import json
import math
import pytz
import re
import sys

from pathlib import Path

def distance(x, y):
    return math.sqrt((x[0] - y[0])**2 + (x[1] - y[1])**2)

def to_feature(source, coordinates):
    return { 
        "type": "Feature", 
        "properties" : { 
            "source": source, 
        }, 
        "geometry": { 
            "type": "LineString", 
            "coordinates": coordinates 
        } 
    }

def gpx_to_geojson(path, target):
    features = []
    for file in Path(path).rglob("*.gpx"):
        with fiona.open(file, layer="track_points") as points:
            coordinates = []
            for point in points:
                c = point["geometry"]["coordinates"]
                if c[0] == 0 or c[1] == 0:
                    print(f"  invalid coordinates: {c}")
                    continue
                if len(coordinates) > 1:
                    c_delta = distance(c, coordinates[-1])
                    if c_delta > 0.05: # ~5km
                        features.append(to_feature(file.name, coordinates))
                        coordinates = []
                coordinates.append(c)
            if len(coordinates) > 1:
                features.append(to_feature(file.name, coordinates))

    with open(target, "w") as out:
        json.dump({ 
            "type": "FeatureCollection", 
            "features": features 
        }, out, indent=2)

if __name__ == "__main__":
    assert len(sys.argv) == 3
    gpx_to_geojson(sys.argv[1], sys.argv[2])
