FROM eclipse-temurin:17-jdk-jammy

WORKDIR /app

# Copy only the sources and runtime configs needed by the Java version.
COPY src/ ./src/
COPY config_*.txt ./

# Compile the package-less Java sources into a dedicated output directory.
RUN mkdir -p out && javac -d out src/*.java

EXPOSE 5000/udp 6000/udp

ENTRYPOINT ["java", "-cp", "/app/out", "Main"]
CMD ["config_A.txt"]