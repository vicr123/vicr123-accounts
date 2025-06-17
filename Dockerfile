FROM alpine:3.21 AS fido-build
RUN apk add --no-cache dotnet8-sdk
COPY vicr123-accounts-fido /usr/src/vicr123-accounts-fido
WORKDIR /usr/src/vicr123-accounts-fido
RUN dotnet restore "vicr123-accounts-fido.csproj"
RUN dotnet publish "vicr123-accounts-fido.csproj" -c Release -o /app/fido/publish

FROM alpine:3.21 AS cpp-build
RUN apk add --no-cache qt6-qtbase-dev cmake gcc g++ make ninja

COPY . /usr/src/vicr123-accounts
WORKDIR /usr/src/vicr123-accounts/build
RUN cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
RUN cmake --build .
RUN DESTDIR=/app/cpp cmake --install .

# Final runtime stage
FROM alpine:3.21 AS final
RUN apk add --no-cache qt6-qtbase qt6-qtbase-postgresql dbus dotnet8-runtime

# Copy built artifacts from build stages
COPY --from=cpp-build /app/cpp /
COPY --from=fido-build /app/fido/publish /app/fido

WORKDIR /app/fido
CMD ["vicr123-accounts"]
