import sys

dim = int(sys.argv[1])

# create topology
for i in xrange(dim * dim):
	open('grid_topology/test2initcosts' + str(i), 'w').close()

with open('grid_topology/topogrid.txt', 'w') as f:
	for i in xrange(dim):
		for j in xrange(dim):
			if j < dim - 1:
				f.write(str(dim * i + j) + ' ' + str(dim * i + j + 1) + '\n')
			if dim * i + j + dim < dim**2:
				f.write(str(dim * i + j) + ' ' + str(dim * i + j + dim) + '\n')

# create shell script
with open('setup_grid.sh', 'w') as f:
	f.write('killall -9 ls_router;\nps;\n')
	f.write('perl make_topology.pl grid_topology/topogrid.txt;\n')
	for i in xrange(dim * dim):
		f.write('./ls_router ' + str(i) + ' grid_topology/test2initcosts' + str(i) + ' grid_topology/test2log' + str(i))
		if i < dim * dim - 1:
			f.write(' & \n')
	f.write('\n')
