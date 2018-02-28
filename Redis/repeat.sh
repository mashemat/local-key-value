a=$1  # old valu of ratio
b=$2  # new value of ratio
sed -i 's/RATIO '$a'/RATIO '$b'/g' redis_client_struct.c
# Give time to Server...
pushd ..
make hiredis-client
popd
