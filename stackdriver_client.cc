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
#include "google/monitoring/v3/metric_service.grpc.pb.h"
#include "stackdriver_client.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using ::google::monitoring::v3::MetricService;
using ::google::monitoring::v3::ListMonitoredResourceDescriptorsRequest;
using ::google::monitoring::v3::CreateMetricDescriptorRequest;
using ::google::monitoring::v3::CreateTimeSeriesRequest;
using ::google::monitoring::v3::TimeSeries;
using ::google::monitoring::v3::TimeInterval;
using ::google::monitoring::v3::Point;
using ::google::api::MetricDescriptor;
using ::google::api::Metric;
using ::google::monitoring::v3::ListMonitoredResourceDescriptorsResponse;

DEFINE_string(project_name, "", "Name of google cloud project for stackdriver");

StackdriverClient::StackdriverClient(std::shared_ptr<Channel> channel)
      : stub_(MetricService::NewStub(channel)) {
  project_name_ = std::string("projects/") + FLAGS_project_name;
}

void StackdriverClient::ListMonitoredResources() {
  ListMonitoredResourceDescriptorsRequest request;
  request.set_name(project_name_);
  request.set_page_size(10);
  ListMonitoredResourceDescriptorsResponse reply;
  ClientContext context;

  Status status = stub_->ListMonitoredResourceDescriptors(
                                  &context, request, &reply);
   if (status.ok()) {
    std::string json_str;
    ::google::protobuf::util::MessageToJsonString(reply, &json_str);
    std::cout << "Response: " << json_str << std::endl;
  } else {
    std::cout << status.error_code() << ":= " << status.error_message()
              << std::endl;
  }
}
using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Reflection;
using google::instrumentation::MeasurementDescriptor;
using google::protobuf::Message;

void StackdriverClient::CreateCanonicalMetricDescriptors(const CanonicalRpcStats& canonical_stats) {
  const Descriptor *desc       = canonical_stats.GetDescriptor();
  const Reflection *refl       = canonical_stats.GetReflection();
  for(int i=0;i<desc->field_count();i++) {
    const FieldDescriptor *field = desc->field(i);
    //fprintf(stderr, "The name of the %i th element is %s and the type is  %s \n",i,field->name().c_str(),field->type_name());
    const Message* n = &refl->GetMessage(canonical_stats, field);
    grpc::instrumentation::v1alpha::CanonicalRpcStats_View test;
    test.CopyFrom(*n);
    std::string name = test.measurement_descriptor().name();
    std::string description = test.measurement_descriptor().description();
    bool has_distribution = test.view().has_distribution_view();
    bool has_interval = test.view().has_interval_view();
    std::cout << "name = " << name << has_distribution << has_interval << std::endl;
#if 0
    // Create request for stackdriver API to create the descriptor
    CreateMetricDescriptorRequest request;
    request.set_name(project_name_);
    auto descriptor = request.mutable_metric_descriptor();
    descriptor->set_name("custom.googleapis.com/" + name);
    descriptor->set_type("custom.googleapis.com/" + name);
    descriptor->set_description(description);
    descriptor->set_display_name(name);
    descriptor->set_metric_kind(::google::api::MetricDescriptor_MetricKind_CUMULATIVE);
    descriptor->set_value_type(::google::api::MetricDescriptor_ValueType_INT64);
    descriptor->set_unit("1");

    ClientContext context;
    MetricDescriptor reply;
    Status status = stub_->CreateMetricDescriptor(&context, request, &reply);
     if (status.ok()) {
      std::string json_str;
      ::google::protobuf::util::MessageToJsonString(reply, &json_str);
      std::cout << "Response: " << json_str << std::endl;
    } else {
      std::cout << "Error: " << status.error_code() << ":= " << status.error_message()
                << std::endl;
    }

#endif
  }
}

void StackdriverClient::CreateAllMetricDescriptors() {
  CreateMetricDescriptorRequest request;
  request.set_name(project_name_);
  auto descriptor = request.mutable_metric_descriptor();
  descriptor->set_name("custom.googleapis.com/client_completed_rpcs");
  descriptor->set_type("custom.googleapis.com/client_completed_rpcs");
  descriptor->set_metric_kind(::google::api::MetricDescriptor_MetricKind_CUMULATIVE);
  descriptor->set_value_type(::google::api::MetricDescriptor_ValueType_INT64);
  descriptor->set_unit("1");
  descriptor->set_description("Number of completed RPCs");
  descriptor->set_display_name("Number of completed RPCs");

  ClientContext context;
  MetricDescriptor reply;
  Status status = stub_->CreateMetricDescriptor(&context, request, &reply);
   if (status.ok()) {
    std::string json_str;
    ::google::protobuf::util::MessageToJsonString(reply, &json_str);
    std::cout << "Response: " << json_str << std::endl;
  } else {
    std::cout << "Error: " << status.error_code() << ":= " << status.error_message()
              << std::endl;
  }
}

void StackdriverClient::AddTimeSeries() {
  CreateTimeSeriesRequest request;
  request.set_name(project_name_);
  TimeSeries *time_series = request.add_time_series();
  auto metric = time_series->mutable_metric();
  metric->set_type("custom.googleapis.com/client_completed_rpcs");
  Point *point = time_series->add_points();
  TimeInterval *interval = point->mutable_interval();
  // Set end time
  auto start_time = interval->mutable_start_time();
  start_time->set_seconds(time(NULL));
  auto end_time = interval->mutable_end_time();
  end_time->set_seconds(time(NULL) + 2);
  auto value = point->mutable_value();
  value->set_int64_value(100);

  ClientContext context;
  google::protobuf::Empty reply;
  Status status = stub_->CreateTimeSeries(&context, request, &reply);
   if (status.ok()) {
    std::cout << "Response: OK" << std::endl;
  } else {
    std::cout << "Error: " << status.error_code() << ":= " << status.error_message()
              << std::endl;
  }
}
