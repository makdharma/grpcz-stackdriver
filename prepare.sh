#!/bin/bash
rm -rf ./google ./grpc
cp -r third_party/googleapis/google ./google
cp -r third_party/grpc-proto/grpc ./grpc
mkdir ./google/instrumentation
cp third_party/instrumentation-proto/stats/census.proto ./google/instrumentation/census.proto
