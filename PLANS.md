# Plans

## All Transit, Efficiently

Really what I want to do is to find the fastest route to visit all stations in a set, using any available public transport and walking. So what I should do is build a graph on that set of vertices, where each edge is a shortest route between those vertices using any forms of transit/walking. I can exclude all the edges that hit stations anywhere in the middle of their route, because these can be formed using the other edges.

What will probably happen is I'll get a graph that looks mostly like the system I'm speedrunning, because the fastest route between adjacent stations in that system is almost always using that system. But there will be some extra edges that "cut across" using other transit systems.

Hopefully this graph will end up small and simple enough that my existing solver can solve it efficiently. If not, the pruning thing that I started working on might help simplify it a bit.

This is going nicely. The next steps are:

* Update the visualization so that it shows the actual trip information on each edge.
* Make it automatically calculate walk/bike connections between nearby stations, probably using crow-flies distance for now, and a pretty short distance cutoff.
* Run it on the entire regional GTFS and see what happens.

Okay that stuff is done and there are some problems:

* A lot of the cuts across are during BART off-hours, so not helpful. I should constrain the cuts across to happen when there is service at the origin and destination stations.
* It's really slow. I subsampled some stuff by about 50x and it took about 1 minute. So I can run it in a hour after I like the results it's producing. But I should look to see if there are any speedups.
* Maybe I can cache some information that is useful across different searches?
* I can do a binary search instead of a linear search for the next departure.
