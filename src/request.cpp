#include "scenario.h"
#include "request.h"

const std::string algorithm = "AWS4-HMAC-SHA256";

namespace cbox {

request::request(conversation &parent) :
  parent_(parent),
  js_env_(parent_.js_env_),
  event_log_(parent_.event_log_) {}

int request::reset(const std::string &raw_host,
                   const Json::Value &request_in)
{
  // connection reset
  conv_conn_.reset(new RestClient::Connection(raw_host));
  if(!conv_conn_) {
    event_log_->error("failed creating resource_conn_ object");
    return 1;
  }
  conv_conn_->SetVerifyPeer(false);
  conv_conn_->SetVerifyHost(false);
  conv_conn_->SetTimeout(30);
  return 0;
}

int request::process(const std::string &raw_host,
                     Json::Value &request_in,
                     Json::Value &requests_out)
{
  int res = 0;

  if((res = reset(raw_host, request_in))) {
    event_log_->error("failed to reset request");
    return res;
  }

  // for
  auto pfor = js_env_.eval_as<uint32_t>(request_in, key_for.c_str(), 1);
  if(!pfor) {
    event_log_->error("failed to read 'for' field");
    return 1;
  }

  for(uint32_t i = 0; i < pfor; ++i) {
    bool error = false;
    {
      Json::Value &request_out = requests_out.append(request_in);
      request_out.removeMember(key_for);

      scenario::stack_scope scope(parent_.parent_,
                                  request_in,
                                  request_out,
                                  error,
                                  scenario::get_default_request_out_options());
      if(error) {
        return 1;
      }

      if(scope.enabled_) {
        parent_.stats_.incr_request_count();
        parent_.parent_.stats_.incr_request_count();

        // method
        auto method = js_env_.eval_as<std::string>(request_in, key_method.c_str());
        if(!method) {
          event_log_->error("failed to read 'method' field");
          return 1;
        }

        // uri
        auto uri = js_env_.eval_as<std::string>(request_in, key_uri.c_str(), "");
        if(!uri) {
          event_log_->error("failed to read 'uri' field");
          return 1;
        }

        // query_string
        auto query_string = js_env_.eval_as<std::string>(request_in, key_query_string.c_str(), "");
        if(!query_string) {
          event_log_->error("failed to read 'query_string' field");
          return 1;
        }

        //data
        auto data = js_env_.eval_as<std::string>(request_in, key_data.c_str(), "");
        if(!data) {
          event_log_->error("failed to read 'data' field");
          return 1;
        }

        //optional auth directive
        auto auth = js_env_.eval_as<std::string>(request_in, key_auth.c_str());
        if(auth) {
          request_out[key_auth] = *auth;
        }

        request_out[key_method] = *method;
        request_out[key_uri] = *uri;
        request_out[key_query_string] = *query_string;
        request_out[key_data] = *data;

        response_mock_ = const_cast<Json::Value *>(request_in.find(key_mock.data(), key_mock.data()+key_mock.length()));

        if((res = execute(*method,
                          auth,
                          *uri,
                          *query_string,
                          *data,
                          request_in,
                          request_out))) {
          return res;
        }
        scope.commit();
      } else {
        request_out.clear();
        request_out[key_enabled] = false;
        break;
      }
    }
    if(error) {
      return 1;
    }
  }

  return res;
}


int request::process_response(const RestClient::Response &resRC,
                              const int64_t rtt,
                              Json::Value &response_in,
                              Json::Value &response_out)
{
  bool error = false;
  {
    scenario::stack_scope scope(parent_.parent_,
                                response_in,
                                response_out,
                                error,
                                scenario::get_default_response_out_options());
    if(error) {
      return 1;
    }

    auto &fopts = scope.out_options_[key_format];

    response_out[key_code] = resRC.code;
    response_out[key_rtt] = utils::from_nano(rtt, utils::from_literal(fopts[key_rtt].asString()));

    Json::Value &headers = response_out[key_headers];
    for(auto &it : resRC.headers) {
      headers[it.first] = it.second;
    }

    if(!resRC.body.empty()) {
      if(fopts[key_body] == "json") {
        try {
          Json::Value body;
          std::istringstream is(resRC.body);
          is >> body;
          response_out[key_body] = body;
        } catch(const Json::RuntimeError &) {
          response_out[key_body] = resRC.body;
        }
      } else {
        response_out[key_body] = resRC.body;
      }
    }

    scope.commit();
  }
  if(error) {
    return 1;
  }

  return 0;
}

int request::execute(const std::string &method,
                     const std::optional<std::string> &auth,
                     const std::string &uri,
                     const std::string &query_string,
                     const std::string &data,
                     Json::Value &request_in,
                     Json::Value &request_out)
{
  int res = 0;
  RestClient::HeaderFields reqHF;

  //read user defined http-headers
  Json::Value *header_node_ptr = nullptr;
  if((header_node_ptr = const_cast<Json::Value *>(request_in.find(key_headers.data(), key_headers.data()+key_headers.length())))) {
    Json::Value::Members keys = header_node_ptr->getMemberNames();
    std::for_each(keys.begin(), keys.end(), [&](const Json::String &it) {
      auto hdr_val = js_env_.eval_as<std::string>(*header_node_ptr, it.c_str(), "");
      if(!hdr_val) {
        res = 1;
      } else {
        reqHF[it] = *hdr_val;
      }
    });
  }
  if(res) {
    return res;
  }

  //invoke http-method
  if(method == "GET") {
    res = get(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "POST") {
    res = post(reqHF, auth, uri, query_string, data, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "PUT") {
    res = put(reqHF, auth, uri, query_string, data, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "DELETE") {
    res = del(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else if(method == "HEAD") {
    res = head(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out);});
  } else {
    event_log_->error("bad method:{}", method);
    res = 1;
  }
  return res;
}

int request::on_response(const RestClient::Response &resRC,
                         const int64_t rtt,
                         Json::Value &request_in,
                         Json::Value &request_out)
{
  int res = 0;
  std::ostringstream os;
  os << resRC.code;

  // update conv stats
  parent_.stats_.incr_categorization(os.str());

  // update scenario stats
  parent_.parent_.stats_.incr_categorization(os.str());

  Json::Value &response_in = request_in[key_response];
  Json::Value &response_out = request_out[key_response];

  res = process_response(resRC,
                         rtt,
                         response_in,
                         response_out);
  return res;
}

// ------------
// --- HTTP ---
// ------------

int request::prepare_http_req(const char *method,
                              const std::optional<std::string> &auth,
                              const std::string &query_string,
                              const std::string &data,
                              std::string &uri_out,
                              RestClient::HeaderFields &reqHF)
{
  if(auth == "aws_v2") {
    parent_.auth_.x_amz_date_ = utils::aws_auth::aws_sign_v2_build_date();
    parent_.auth_.aws_sign_v2_build(method, uri_out, reqHF);
  } else if(auth == "aws_v4") {
    parent_.auth_.x_amz_date_ = utils::aws_auth::aws_sign_v4_build_date();
    parent_.auth_.aws_sign_v4_build(method,
                                    uri_out,
                                    query_string,
                                    data,
                                    reqHF);
  }
  conv_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string != "") {
    uri_out += '?';
    uri_out += query_string;
  }

  return 0;
}

int request::post(RestClient::HeaderFields &reqHF,
                  const std::optional<std::string> &auth,
                  const std::string &uri,
                  const std::string &query_string,
                  const std::string &data,
                  const std::function <int (const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req("POST", auth, query_string, data, luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_) {
    if((res = mocked_to_res(resRC))) {
      return res;
    }
  } else {
    resRC = conv_conn_->post(luri, data);
  }
  std::chrono::duration lrtt = std::chrono::system_clock::now() - t0;

  res = cb(resRC, lrtt.count());
  return res;
}

int request::put(RestClient::HeaderFields &reqHF,
                 const std::optional<std::string> &auth,
                 const std::string &uri,
                 const std::string &query_string,
                 const std::string &data,
                 const std::function <int (const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req("PUT", auth, query_string, data, luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_) {
    if((res = mocked_to_res(resRC))) {
      return res;
    }
  } else {
    resRC = conv_conn_->put(luri, data);
  }
  std::chrono::duration lrtt = std::chrono::system_clock::now() - t0;

  res = cb(resRC, lrtt.count());
  return res;
}

int request::get(RestClient::HeaderFields &reqHF,
                 const std::optional<std::string> &auth,
                 const std::string &uri,
                 const std::string &query_string,
                 const std::function <int (const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req("GET", auth, query_string, "", luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_) {
    if((res = mocked_to_res(resRC))) {
      return res;
    }
  } else {
    resRC = conv_conn_->get(luri);
  }
  std::chrono::duration lrtt = std::chrono::system_clock::now() - t0;

  res = cb(resRC, lrtt.count());
  return res;
}

int request::del(RestClient::HeaderFields &reqHF,
                 const std::optional<std::string> &auth,
                 const std::string &uri,
                 const std::string &query_string,
                 const std::function <int (const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req("DELETE", auth, query_string, "", luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_) {
    if((res = mocked_to_res(resRC))) {
      return res;
    }
  } else {
    resRC = conv_conn_->del(luri);
  }
  std::chrono::duration lrtt = std::chrono::system_clock::now() - t0;

  res = cb(resRC, lrtt.count());
  return res;
}

int request::head(RestClient::HeaderFields &reqHF,
                  const std::optional<std::string> &auth,
                  const std::string &uri,
                  const std::string &query_string,
                  const std::function <int (const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req("HEAD", auth, query_string, "", luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_) {
    if((res = mocked_to_res(resRC))) {
      return res;
    }
  } else {
    resRC = conv_conn_->head(luri);
  }
  std::chrono::duration lrtt = std::chrono::system_clock::now() - t0;

  res = cb(resRC, lrtt.count());
  return res;
}

// -------------
// --- UTILS ---
// -------------

void request::dump_hdr(const RestClient::HeaderFields &hdr) const
{
  std::for_each(hdr.begin(), hdr.end(), [&](auto it) {
    event_log_->trace("{}:{}", it.first, it.second);
  });
}

int request::mocked_to_res(RestClient::Response &resRC)
{
  //code
  auto code = js_env_.eval_as<int32_t>(*response_mock_, key_code.c_str());
  if(code) {
    resRC.code = *code;
  }

  //body
  auto body = js_env_.eval_as<std::string>(*response_mock_, key_body.c_str());
  if(body) {
    resRC.body = *body;
  }

  return 0;
}

}
