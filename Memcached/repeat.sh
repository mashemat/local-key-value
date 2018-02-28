
a=$1  # old valu of ratio
b=$2  # new value of ratio
sed -i 's/RATIO '$a'/RATIO '$b'/g' memcached.c
# Give time to Server...
make memcached


