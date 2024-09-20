FROM fedora:40 AS fido
RUN dnf install dotnet-sdk-8.0 git -y
RUN git clone https://github.com/vicr123/vicr123-accounts.git
WORKDIR /vicr123-accounts
RUN git checkout fido-support
RUN dotnet restore "vicr123-accounts-fido.csproj"
RUN dotnet build "vicr123-accounts-fido.csproj" -c Release -o /app/fido/build
RUN dotnet publish "vicr123-accounts-fido.csproj" -c Release -o /app/fido/publish

FROM fedora:40 AS final

RUN dnf install qt6-qtbase-devel qt6-qtbase-postgresql dbus-daemon dotnet-runtime-8.0 -y
COPY . /usr/src/vicr123-accounts
WORKDIR /usr/src/vicr123-accounts
RUN mkdir build
WORKDIR /usr/src/vicr123-accounts/build
RUN cmake ..
RUN cmake --build .
RUN cmake --install .

RUN dnf clean all

WORKDIR /app/fido
COPY --from=fido /app/fido/publish .
CMD ["vicr123-accounts"]
