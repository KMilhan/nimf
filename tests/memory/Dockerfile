ARG BASE_IMAGE=nimf-mozc-ubuntu:test
FROM ${BASE_IMAGE}
USER root
RUN make -C /src/libnimf clean && make -C /src/modules clean && make -C /src/bin clean
COPY . /src
ARG CHECK_MODE=sanitize
ENV NIMF_CHECK_MODE=${CHECK_MODE}
RUN bash tests/memory/build.sh
USER tester
ENTRYPOINT []
CMD ["bash", "/src/tests/memory/session.sh"]
