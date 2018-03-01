a=`pgrep main`
echo $a
kill -9 $a
./run-servers.sh $1
