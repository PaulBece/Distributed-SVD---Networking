# Distributed-SVD---Networking

# Install libraries
sudo apt-get install libeigen3-dev

# Compile
g++ -o src/client.exe src/client.cpp -I /usr/include/eigen3 -std=c++17
g++ -o src/main-server.exe src/main-server.cpp -I /usr/include/eigen3 -std=c++17
g++ -o src/processing-server.exe src/processing-server.cpp -I /usr/include/eigen3 -std=c++17