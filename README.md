# Build & run

1. `git submodule update --init --recursive`
2. `docker build --build-arg THREADS=4 --target runtime .` \
where `THREADS` is number of threads to build seastar, default is 2
3. `docker run -p 1270:1270 -it --rm kv_store`
4. `./kv_store`