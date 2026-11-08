ARG BASE_IMAGE=nimf-latency-ubuntu:test
FROM ${BASE_IMAGE}
USER root
RUN make -C /src/libnimf clean && make -C /src/modules clean && make -C /src/bin clean
ENV CFLAGS="-g -O3" CXXFLAGS="-g -O3" LDFLAGS=""
# Keep the latency driver and GTK fixtures from the O2 image unchanged.
RUN bash tests/mozc/build.sh
USER tester
