# Reproducible, non-root build for the Nokia E51 (S60 3rd FP1) task manager.
#
# The proprietary Nokia SDK is downloaded over HTTPS from the Internet Archive
# mirror and pinned by SHA-256. Its redistribution is restricted by the Nokia
# EULA; this image is for building locally from a legally obtained copy only.
FROM i386/debian:bullseye-slim

ENV DEBIAN_FRONTEND=noninteractive

ARG GNUPOC_URL=https://www.martin.st/symbian/gnupoc-package-1.22.tar.gz
ARG GNUPOC_SHA256=32c9e88eeeff53a5baf02f208e03ab9c42dc28e68991d761059807a28f8058ec
ARG CSLGCC_URL=https://www.martin.st/symbian/gnu-csl-arm-2005Q1C-arm-none-symbianelf-i686-pc-linux-gnu.tar.bz2
ARG CSLGCC_SHA256=cd80c3068b69d2f7ebd9dc917cdd6353158d465623e8ec4ac244915143ce5ce0
ARG SDK_URL=https://archive.org/download/nokia_sdks_n_dev_tools/S60-SDK-200634-3.1-Cpp-f.1090b.zip
ARG SDK_SHA256=f445cf1f7ddaa5ec5d5a8a806271db8880c9a430150b1cfcdc7de9d9bd758858

# Bullseye moved to the Debian archive; signed packages keep apt reproducible.
RUN printf 'deb http://archive.debian.org/debian bullseye main\n' > /etc/apt/sources.list \
    && printf 'deb http://archive.debian.org/debian-security bullseye-security main\n' >> /etc/apt/sources.list \
    && apt-get -o Acquire::Check-Valid-Until=false update \
    && apt-get install -y --no-install-recommends \
        ca-certificates curl unzip file make g++ perl patch bzip2 xz-utils \
        openssl zlib1g-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --uid 10001 --shell /bin/sh builder

WORKDIR /opt

# GnuPoc patches + native replacements for the Symbian command-line tools.
RUN curl -fsSL "$GNUPOC_URL" -o /tmp/gnupoc.tar.gz \
    && echo "$GNUPOC_SHA256  /tmp/gnupoc.tar.gz" | sha256sum -c - \
    && tar -xzf /tmp/gnupoc.tar.gz -C /opt \
    && rm /tmp/gnupoc.tar.gz

# GnuPoc 1.22 predates the OpenSSL 1.1 API changes; patch makesis/signsis.
COPY docker/gnupoc-openssl11.patch /opt/gnupoc-openssl11.patch
RUN cd /opt/gnupoc-package-1.22 \
    && patch -p1 < /opt/gnupoc-openssl11.patch \
    && sed -i 's/verify(InRange/assert(InRange/' tools/elf2e32/deflate/deflate/farray.h \
    && perl -0pi -e 's/func == PEM_F_PEM_READ_BIO && //' tools/makesis-4/src/signutils.cpp \
    && perl -0pi -e 's/func == ASN1_F_ASN1_GET_OBJECT && //' tools/makesis-4/src/signutils.cpp \
    && perl -0pi -e 's/int func = ERR_GET_FUNC\(err\);/int func = 0; (void)func;/' tools/makesis-4/src/signutils.cpp \
    && rm /opt/gnupoc-openssl11.patch

# CodeSourcery GCCE cross-compiler (prebuilt IA32 GNU/Linux binaries).
RUN curl -fsSL "$CSLGCC_URL" -o /tmp/csl-gcc.tar.bz2 \
    && echo "$CSLGCC_SHA256  /tmp/csl-gcc.tar.bz2" | sha256sum -c - \
    && mkdir -p /opt/symbian-tools \
    && tar -xjf /tmp/csl-gcc.tar.bz2 -C /opt/symbian-tools \
    && rm /tmp/csl-gcc.tar.bz2

# Build native (no Wine) replacements for rcomp, makesis, signsis, elf2e32, ...
# -fpermissive is needed by this 2012 source with modern g++.
ENV CFLAGS="-fpermissive -D__alloca=__builtin_alloca"
RUN cd /opt/gnupoc-package-1.22/tools \
    && chmod +x install_eka2_tools \
    && ./install_eka2_tools /opt/symbian-tools

# Install the S60 3rd Edition FP1 SDK itself.
RUN curl -fL --retry 3 "$SDK_URL" -o /tmp/sdk.zip \
    && echo "$SDK_SHA256  /tmp/sdk.zip" | sha256sum -c - \
    && cd /opt/gnupoc-package-1.22/sdks \
    && chmod +x unshield/unshield lowercase \
    && ./install_gnupoc_s60_31 /tmp/sdk.zip /opt/symbian-sdks/s60_31 \
    && rm -rf /tmp/sdk.zip /opt/gnupoc-package-1.22/sdks/_e

COPY docker/fix-sdk-perl.sh /opt/fix-sdk-perl.sh
RUN EPOCROOT=/opt/symbian-sdks/s60_31/ sh /opt/fix-sdk-perl.sh \
    && rm /opt/fix-sdk-perl.sh

# The build scripts write into epoc32/build, so the unprivileged builder
# needs to own both the SDK and the cross tools.
RUN chown -R builder:builder /opt/symbian-sdks /opt/symbian-tools

ENV EPOCROOT=/opt/symbian-sdks/s60_31/
# /usr/bin goes before epoc32/tools because that directory contains a folder
# literally named "perl", which sh would otherwise try to execute.
ENV PATH=/opt/symbian-tools/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/symbian-sdks/s60_31/epoc32/tools

COPY docker/build-in-container.sh /usr/local/bin/build-in-container.sh
RUN chmod 0755 /usr/local/bin/build-in-container.sh \
    && mkdir -p /work /out && chown builder:builder /work /out

USER builder
WORKDIR /work

# Mount the repo read-only at /src and the output directory at /out:
#   docker run --rm -v "$PWD:/src:ro" -v "$PWD/out:/out" IMAGE
ENTRYPOINT ["/usr/local/bin/build-in-container.sh"]
