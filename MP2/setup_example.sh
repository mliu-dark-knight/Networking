killall -9 ls_router;
ps;
perl make_topology.pl example_topology/topoexample.txt;
./ls_router 0 example_topology/test2initcosts0 example_topology/test2log0 &
./ls_router 1 example_topology/test2initcosts1 example_topology/test2log1 &
./ls_router 2 example_topology/test2initcosts2 example_topology/test2log2 &
./ls_router 3 example_topology/test2initcosts3 example_topology/test2log3 &
./ls_router 4 example_topology/test2initcosts4 example_topology/test2log4 &
./ls_router 5 example_topology/test2initcosts5 example_topology/test2log5 &
./ls_router 6 example_topology/test2initcosts6 example_topology/test2log6 &
./ls_router 7 example_topology/test2initcosts7 example_topology/test2log7 &
./ls_router 255 example_topology/test2initcosts255 example_topology/test2log255
