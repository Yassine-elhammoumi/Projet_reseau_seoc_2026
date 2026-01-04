#!/bin/bash

cd ..

echo "Retrieve test"

curl -i -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/index.html | grep -q "200" && echo "OK" || echo "Not OK"
curl -i -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/test.html | grep -q "200" && echo "OK" || echo "Not OK"
curl -i -s -o /dev/null -w "%{http_code}\n" http://localhost:8080/big.html | grep -q "200" && echo "OK" || echo "Not OK"

echo "Length test"

h1=$(curl -s -D - -o /dev/null http://localhost:8080/index.html | grep -i Content-Length | tr -dc '0-9')
b1=$(curl -s http://localhost:8080/index.html | wc -c)
[ "$h1" -eq "$b1" ] && echo OK || echo "NOT OK: $h1 != $b1"

h2=$(curl -s -D - -o /dev/null http://localhost:8080/test.html | grep -i Content-Length | tr -dc '0-9')
b2=$(curl -s http://localhost:8080/test.html | wc -c)
[ "$h2" -eq "$b2" ] && echo OK || echo "NOT OK: $h2 != $b2"

h3=$(curl -s -D - -o /dev/null http://localhost:8080/big.html | grep -i Content-Length | tr -dc '0-9')
b3=$(curl -s http://localhost:8080/big.html | wc -c)
[ "$h3" -eq "$b3" ] && echo OK || echo "NOT OK: $h3 != $b3"



sudo lsof -t -i:8081 -i:8080 -i:9000 | xargs -r sudo kill -9 || true
