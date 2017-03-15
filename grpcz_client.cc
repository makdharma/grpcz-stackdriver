/*
 *
 * Copyright 2017, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <iostream>
#include <memory>
#include <string>

#include <google/protobuf/util/json_util.h>
#include <grpc++/grpc++.h>
#include <grpc/support/log.h>

#include "gflags/gflags.h"
#include "mongoose.h"

#include "google/instrumentation/census.grpc.pb.h"
#include "grpc/instrumentation/v1alpha/monitoring.grpc.pb.h"
#include "google/monitoring/v3/metric_service.grpc.pb.h"
#include "stackdriver_client.h"

DEFINE_string(sdserver, "monitoring.googleapis.com:443",
              "file path (or host:port) where stackdriver server is running");
DEFINE_string(server, "127.0.0.1:8080",
              "file path (or host:port) where grpcz server is running");
DEFINE_string(http_port, "8000",
              "Port id for accessing the HTTP server that renders /grpcz page");
DEFINE_bool(print, false, "only print the output and quit");
DEFINE_string(roots_pem, "", "sets env var GRPC_DEFAULT_SSL_ROOTS_FILE_PATH");
DEFINE_string(credentials, "", "sets env var GOOGLE_APPLICATION_CREDENTIALS");

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

using ::grpc::instrumentation::v1alpha::CanonicalRpcStats;
using ::grpc::instrumentation::v1alpha::Monitoring;
using ::google::monitoring::v3::MetricService;
using ::google::monitoring::v3::ListMonitoredResourceDescriptorsRequest;
using ::google::monitoring::v3::ListMonitoredResourceDescriptorsResponse;

static const std::string static_html_header =
    "<!DOCTYPE html> <html> <head> <style> \
table { border-collapse: collapse; width: 100%; } \
table, td, th { border: 1px solid black; } \
</style> </head> <body>\
<div id='stats' stats='";

static const std::string static_html_footer =
    "' class='hidden'></div>\
<h1> GRPCZ FTW </h1> <div id='table'> </div> \
<script> \
  var canonical_stats = JSON.parse(\
            document.getElementById('stats').getAttribute('stats')); \
  var table = document.createElement('table'); \
  for (var key in canonical_stats) { \
    name = canonical_stats[key]['view']['viewName']; \
    distribution = canonical_stats[key]['view']['distributionView']; \
    interval = canonical_stats[key]['view']['intervalView']; \
    value = (interval == undefined) ? \
      JSON.stringify(distribution, null, ' ') : \
      JSON.stringify(interval, null, ' '); \
    var row = table.insertRow(-1); \
    var col1 = row.insertCell(0); \
    var col2 = row.insertCell(1); \
    col1.innerHTML = name; \
    col2.innerHTML = '<pre>' + value + '</pre>'; \
  } \
  document.getElementById('table').appendChild(table); \
</script> </body> </html>";

class GrpczClient {
 public:
  GrpczClient(std::shared_ptr<Channel> channel)
      : stub_(Monitoring::NewStub(channel)) {}

  void GetCanonicalRpcStats(CanonicalRpcStats *reply) {
    const ::google::protobuf::Empty request;
    ClientContext context;
    Status status = stub_->GetCanonicalRpcStats(&context, request, reply);
    if (!status.ok()) {
      std::cout << "Error: " << status.error_code() << ":= " << status.error_message()
                << std::endl;
    }
  }

  std::string GetStatsAsJson() {
    const ::google::protobuf::Empty request;
    CanonicalRpcStats reply;
    ClientContext context;
    Status status = stub_->GetCanonicalRpcStats(&context, request, &reply);

    if (status.ok()) {
      std::string json_str;
      ::google::protobuf::util::MessageToJsonString(reply, &json_str);
      return json_str;
    } else {
      static const std::string error_message_json =
          "{\"grpcz Access Error\"\
      :{\"view\":{\"viewName\":\"grpcz Access Error\",\
      \"intervalView\":\"Server not running?\"}}}";
      std::cout << status.error_code() << ":= " << status.error_message()
                << std::endl;
      return error_message_json;
    }
  }

 private:
  std::unique_ptr<Monitoring::Stub> stub_;
};

static struct mg_serve_http_opts s_http_server_opts;
std::unique_ptr<GrpczClient> g_grpcz_client;
std::unique_ptr<StackdriverClient> g_monitoring_client;

static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    mg_serve_http(nc, (struct http_message *)p, s_http_server_opts);
  }
}

static void grpcz_handler(struct mg_connection *nc, int ev, void *ev_data) {
  (void)ev;
  (void)ev_data;
  gpr_log(GPR_INFO, "fetching grpcz stats from %s", FLAGS_server.c_str());
  std::string json_str = g_grpcz_client->GetStatsAsJson();
  std::string rendered_html =
      static_html_header + json_str + static_html_footer;
  mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n%s", rendered_html.c_str());
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
#if 1
  CanonicalRpcStats canonical_stats;
  g_grpcz_client.reset(new GrpczClient(
      grpc::CreateChannel(FLAGS_server, grpc::InsecureChannelCredentials())));
  g_grpcz_client->GetCanonicalRpcStats(&canonical_stats);

  // Create a channel to stackdriver
  setenv("GRPC_DEFAULT_SSL_ROOTS_FILE_PATH", FLAGS_roots_pem.c_str(), 1);
  setenv("GOOGLE_APPLICATION_CREDENTIALS", FLAGS_credentials.c_str(), 1);
  g_monitoring_client.reset(new StackdriverClient(
      ::grpc::CreateChannel(FLAGS_sdserver, grpc::GoogleDefaultCredentials())));
  //g_monitoring_client->ListMonitoredResources();
  //g_monitoring_client->CreateAllMetricDescriptors();
  //g_monitoring_client->AddTimeSeries();
  g_monitoring_client->CreateCanonicalMetricDescriptors(canonical_stats);
  return 0;
#endif
#if 1
  // Create a channel to the /grpcz server
  g_grpcz_client.reset(new GrpczClient(
      grpc::CreateChannel(FLAGS_server, grpc::InsecureChannelCredentials())));
  if (FLAGS_print) {
    g_grpcz_client->GetStatsAsJson();
    return 0;
  }

  // Set up a mongoose webserver handler
  struct mg_mgr mgr;
  mg_mgr_init(&mgr, NULL);
  gpr_log(GPR_INFO, "Starting grpcz web server on port %s\n",
          FLAGS_http_port.c_str());

  struct mg_connection *nc = mg_bind(&mgr, FLAGS_http_port.c_str(), ev_handler);
  if (nc == NULL) {
    gpr_log(GPR_ERROR, "Failed to create listener on port %s\n",
            FLAGS_http_port.c_str());
    return -11;
  }
  mg_register_http_endpoint(nc, "/grpcz", grpcz_handler);
  mg_set_protocol_http_websocket(nc);

  // Poll in a loop and serve /grpcz pages
  for (;;) {
    mg_mgr_poll(&mgr, 100);
  }
  mg_mgr_free(&mgr);
  return 0;
#endif
}
