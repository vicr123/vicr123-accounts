FROM fedora:44 AS fido
RUN dnf install dotnet-sdk-10.0 git -y
RUN git clone https://github.com/vicr123/vicr123-accounts.git
WORKDIR /vicr123-accounts
RUN git checkout fido-support
RUN dotnet restore "vicr123-accounts-fido.csproj"
RUN dotnet build "vicr123-accounts-fido.csproj" -c Release -o /app/fido/build
RUN dotnet publish "vicr123-accounts-fido.csproj" -c Release -o /app/fido/publish

FROM fedora:44 AS rust
WORKDIR /usr/src/vicr123-accounts
COPY . .

RUN dnf install -y rust cargo
RUN cargo install --path .

FROM fedora:44 AS final

RUN dnf install dbus-daemon dotnet-runtime-10.0 -y
COPY --from=rust /root/.cargo/bin /usr/bin

WORKDIR /app/fido
COPY --from=fido /app/fido/publish .
ENV FIDO_HELPER_PATH=/app/fido/publish/vicr123-accounts-fido

CMD ["vicr123-accounts"]