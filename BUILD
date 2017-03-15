
package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "grpcz_client",
    srcs = [
        "grpcz_client.cc",
        "stackdriver_client.cc",
        "stackdriver_client.h",
    ],
    deps = [
        "//grpc/instrumentation/v1alpha:monitoring",
        "//google/monitoring/v3:metric_service",
        "@com_github_gflags_gflags//:gflags",
        "@mongoose_repo//:mongoose_lib",
    ],
)
