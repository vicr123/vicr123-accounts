FROM fedora:36
RUN dnf install qt6-qtbase-devel qt6-qtbase-postgresql dbus-daemon -y
COPY . /usr/src/vicr123-accounts
WORKDIR /usr/src/vicr123-accounts
RUN mkdir build
WORKDIR /usr/src/vicr123-accounts/build
RUN cmake ..
RUN cmake --build .
RUN cmake --install .
CMD ["vicr123-accounts"]
