ARG BASE_IMAGE=nimf-mozc-ubuntu:test
FROM ${BASE_IMAGE}
USER root
RUN make -C /src/libnimf clean && make -C /src/modules clean && make -C /src/bin clean
COPY . /src
ENV CFLAGS="-g -O2" CXXFLAGS="-g -O2" LDFLAGS=""
ARG NIMF_BUILD_PROFILING=0
RUN bash tests/mozc/build.sh && bash tests/latency/build.sh
USER tester
ENTRYPOINT ["bash", "/src/tests/latency/session.sh"]
