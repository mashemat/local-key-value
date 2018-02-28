#$server iP   port

port_map=(11211 11212 11213 11214 11215 11216 11217 11218 11219 11220 11221 11222 11223 11224 11225 11226)
#for i in `seq 1 16`
#do
#echo $i

#echo ${port_map[$i]}
#./initial.run  $1 ${port_map[$i]}
./initial.run  $1 11227
sleep 1
#done

