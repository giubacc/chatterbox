#include "scenario.h"
#include "conversation.h"
#include "request.h"

#define ERR_FAIL_RESET_REQ    "failed to reset request"
#define ERR_FAIL_READ_FOR     "failed to read 'for'"
#define ERR_FAIL_READ_METHOD  "failed to read 'method'"
#define ERR_BAD_METHOD        "bad 'method'"
#define ERR_FAIL_READ_URI     "failed to read 'uri'"

const std::string algorithm = "AWS4-HMAC-SHA256";

namespace cbox {

request::request(conversation &parent) : parent_(parent),
  scen_out_p_resolv_(parent_.scen_out_p_resolv_),
  scen_p_evaluator_(parent_.scen_p_evaluator_),
  indexed_nodes_map_(parent_.indexed_nodes_map_),
  js_env_(parent_.js_env_),
  event_log_(parent_.event_log_) {}

int request::reset(const std::string &raw_host,
                   ryml::NodeRef request_in)
{
  // connection reset
  conv_conn_.reset(new RestClient::Connection(raw_host));
  conv_conn_->SetVerifyPeer(false);
  conv_conn_->SetVerifyHost(false);
  conv_conn_->SetTimeout(30);
  return 0;
}

int request::process(const std::string &raw_host,
                     ryml::NodeRef request_in,
                     ryml::NodeRef requests_out)
{
  int res = 0;

  if((res = reset(raw_host, request_in))) {
    event_log_->error(ERR_FAIL_RESET_REQ);
    return res;
  }

  // for
  bool further_eval = false;
  auto pfor = js_env_.eval_as<uint32_t>(request_in,
                                        key_for,
                                        1,
                                        true,
                                        nullptr,
                                        PROP_EVAL_RGX,
                                        &further_eval);
  if(!pfor) {
    if(further_eval) {
      pfor = scen_p_evaluator_.eval_as<uint32_t>(request_in,
                                                 key_for,
                                                 scen_out_p_resolv_);
    }
    if(!pfor) {
      event_log_->error(ERR_FAIL_READ_FOR);
      return 1;
    }
  }

  for(uint32_t i = 0; i < pfor; ++i) {
    bool error = false;
    {
      ryml::NodeRef request_out = requests_out.append_child();
      request_out |= ryml::MAP;
      utils::set_tree_node(*request_in.tree(),
                           request_in,
                           request_out,
                           ryml_request_out_buf_);

      if(request_out.has_child(key_for)) {
        request_out.remove_child(key_for);
      }

      scenario::stack_scope scope(parent_.parent_,
                                  request_in,
                                  request_out,
                                  error,
                                  utils::get_default_request_out_options());
      if(error) {
        return 1;
      }

      if(scope.enabled_) {
        parent_.stats_.incr_request_count();
        parent_.parent_.stats_.incr_request_count();

        //id
        further_eval = false;
        auto id = js_env_.eval_as<std::string>(request_in,
                                               key_id,
                                               std::nullopt,
                                               true,
                                               nullptr,
                                               PROP_EVAL_RGX,
                                               &further_eval);
        if(!id) {
          if(further_eval) {
            id = scen_p_evaluator_.eval_as<std::string>(request_in,
                                                        key_id,
                                                        scen_out_p_resolv_);
          }
        }
        if(id) {
          indexed_nodes_map_[*id] = request_out;
        }

        // method
        further_eval = false;
        auto method = js_env_.eval_as<std::string>(request_in,
                                                   key_method,
                                                   std::nullopt,
                                                   true,
                                                   nullptr,
                                                   PROP_EVAL_RGX,
                                                   &further_eval);
        if(!method) {
          if(further_eval) {
            method = scen_p_evaluator_.eval_as<std::string>(request_in,
                                                            key_method,
                                                            scen_out_p_resolv_);
          }
          if(!method) {
            res = 1;
            event_log_->error(ERR_FAIL_READ_METHOD);
            utils::clear_map_node_put_key_val(request_out, key_error, ERR_FAIL_READ_METHOD);
          }
        }
        request_out.remove_child(key_method);
        request_out[key_method] << *method;

        // uri
        further_eval = false;
        std::optional<std::string> uri;
        if(!res) {
          uri = js_env_.eval_as<std::string>(request_in,
                                             key_uri,
                                             std::nullopt,
                                             true,
                                             nullptr,
                                             PROP_EVAL_RGX,
                                             &further_eval);
          if(!uri) {
            if(further_eval) {
              uri = scen_p_evaluator_.eval_as<std::string>(request_in,
                                                           key_uri,
                                                           scen_out_p_resolv_);
            }
            if(!uri) {
              res = 1;
              event_log_->error(ERR_FAIL_READ_URI);
              utils::clear_map_node_put_key_val(request_out, key_error, ERR_FAIL_READ_URI);
            }
          }
          request_out.remove_child(key_uri);
          request_out[key_uri] << *uri;
        }

        // query_string
        further_eval = false;
        std::optional<std::string> query_string;
        if(!res) {
          query_string = js_env_.eval_as<std::string>(request_in,
                                                      key_query_string,
                                                      std::nullopt,
                                                      true,
                                                      nullptr,
                                                      PROP_EVAL_RGX,
                                                      &further_eval);
          if(further_eval) {
            query_string = scen_p_evaluator_.eval_as<std::string>(request_in,
                                                                  key_query_string,
                                                                  scen_out_p_resolv_);
            if(!query_string) {
              res = 1;
              event_log_->error(ERR_FAIL_EVAL);
              utils::clear_map_node_put_key_val(request_out, key_error, ERR_FAIL_EVAL);
            }
          }
          if(query_string) {
            request_out.remove_child(key_query_string);
            request_out[key_query_string] << *query_string;
          }
        }

        // data
        further_eval = false;
        std::optional<std::string> data;
        bool is_error = false;
        if(!res) {
          data = js_env_.eval_as<std::string>(request_in,
                                              key_data,
                                              std::nullopt,
                                              false,
                                              &is_error,
                                              PROP_EVAL_RGX,
                                              &further_eval);
          if(is_error && request_in.has_child(key_data)) {
            ryml::NodeRef node_data_in = request_in[key_data];

            ryml::Tree tree_data;
            ryml::NodeRef td_root = tree_data.rootref();
            td_root |= ryml::MAP;
            utils::set_tree_node(*node_data_in.tree(),
                                 node_data_in,
                                 td_root,
                                 ryml_request_out_buf_);

            ryml::NodeRef node_data = td_root[key_data];
            node_data.clear_key();

            std::ostringstream os;
            os << node_data;
            data.emplace(os.str());
          }
          if(further_eval) {
            data = scen_p_evaluator_.eval_as<std::string>(request_in,
                                                          key_data,
                                                          scen_out_p_resolv_);
            if(!data) {
              res = 1;
              event_log_->error(ERR_FAIL_EVAL);
              utils::clear_map_node_put_key_val(request_out, key_error, ERR_FAIL_EVAL);
            }
          }
          if(data) {
            request_out.remove_child(key_data);
            request_out[key_data] << *data;
          }
        }

        // auth
        further_eval = false;
        std::optional<std::string> auth;
        if(!res) {
          auth = js_env_.eval_as<std::string>(request_in,
                                              key_auth,
                                              std::nullopt,
                                              true,
                                              nullptr,
                                              PROP_EVAL_RGX,
                                              &further_eval);
          if(further_eval) {
            auth = scen_p_evaluator_.eval_as<std::string>(request_in,
                                                          key_auth,
                                                          scen_out_p_resolv_);
            if(!auth) {
              res = 1;
              event_log_->error(ERR_FAIL_EVAL);
              utils::clear_map_node_put_key_val(request_out, key_error, ERR_FAIL_EVAL);
            }
          }
          if(auth) {
            request_out.remove_child(key_auth);
            request_out[key_auth] << *auth;
          }
        }

        // mock
        if(!res && request_in.has_child(key_mock)) {
          response_mock_ = request_in[key_mock];
        }

        if(!res && (res = execute(*method,
                                  auth,
                                  *uri,
                                  query_string,
                                  data,
                                  request_in,
                                  request_out)));
        if(!res) {
          scope.commit();
        } else {
          break;
        }
      } else {
        utils::clear_map_node_put_key_val(request_out, key_enabled, STR_FALSE);
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
                              ryml::NodeRef response_in,
                              ryml::NodeRef response_out)
{
  bool error = false;
  {
    scenario::stack_scope scope(parent_.parent_,
                                response_in,
                                response_out,
                                error,
                                utils::get_default_response_out_options());
    if(error) {
      return 1;
    }

    ryml::ConstNodeRef out_opts_root = scope.out_opts_.rootref();
    ryml::ConstNodeRef fopts = out_opts_root[key_format];

    response_out[key_code] << resRC.code;
    std::string rttf;
    fopts[key_rtt] >> rttf;
    response_out[key_rtt] << utils::from_nano(rtt, utils::from_literal(rttf));

    if(!resRC.headers.empty()) {
      ryml::NodeRef headers = response_out[key_headers];
      headers |= ryml::MAP;
      for(auto &it : resRC.headers) {
        ryml::csubstr header = headers.to_arena(it.first);
        headers[header] << it.second;
      }
    }

    if(resRC.code != CURLE_GOT_NOTHING && !resRC.body.empty()) {
      if(fopts[key_body] == STR_JSON) {
        ryml::set_callbacks(parent_.parent_.ctx_.REH_.callbacks());
        parent_.parent_.ctx_.REH_.check_error_occurs([&] {
          std::stringstream ss;
          ss << resRC.body;
          ryml::Tree body = ryml::parse_in_arena(ryml::to_csubstr(ss.str()));
          ryml::NodeRef response_body = response_out[key_body];
          response_body |= ryml::MAP;
          utils::set_tree_node(body,
                               body.rootref(),
                               response_body,
                               ryml_request_out_buf_);
        },
        [&](std::runtime_error const &e) {
          response_out[key_body] << resRC.body |= ryml::KEYVAL;
        });
        ryml::set_callbacks(parent_.parent_.ctx_.REH_.defaults);
      } else {
        response_out[key_body] << resRC.body |= ryml::KEYVAL;
      }
    }

    if(error) {
      return 1;
    }
    scope.commit();
    return 0;
  }
}

int request::execute(const std::string &method,
                     const std::optional<std::string> &auth,
                     const std::string &uri,
                     const std::optional<std::string> &query_string,
                     const std::optional<std::string> &data,
                     ryml::NodeRef request_in,
                     ryml::NodeRef request_out)
{
  int res = 0;
  RestClient::HeaderFields reqHF;

  // read user defined http-headers
  if(request_out.has_child(key_headers)) {
    ryml::Tree rendered_headers;
    ryml::NodeRef rh_root = rendered_headers.rootref();
    rh_root |= ryml::MAP;

    ryml::NodeRef header_node = request_out[key_headers];
    for(ryml::NodeRef hdr : header_node.children()) {
      std::ostringstream os;
      os << hdr.key();
      auto hdr_val = js_env_.eval_as<std::string>(header_node, os.str().c_str(), "");
      if(!hdr_val) {
        res = 1;
      } else {
        auto key = os.str();
        reqHF[key.c_str()] = *hdr_val;
        ryml::csubstr arena_key = rh_root.to_arena(key);
        rh_root[arena_key] << *hdr_val;
      }
    }
    header_node.clear_children();
    utils::set_tree_node(rendered_headers,
                         rh_root,
                         header_node,
                         ryml_request_out_buf_);
  }

  if(res) {
    return res;
  }

  // invoke http-method
  if(method == HTTP_GET) {
    res = get(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out); });
  } else if(method == HTTP_POST) {
    res = post(reqHF, auth, uri, query_string, data, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out); });
  } else if(method == HTTP_PUT) {
    res = put(reqHF, auth, uri, query_string, data, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out); });
  } else if(method == HTTP_DELETE) {
    res = del(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out); });
  } else if(method == HTTP_HEAD) {
    res = head(reqHF, auth, uri, query_string, [&](const RestClient::Response &res, const int64_t rtt) -> int {
      return on_response(res, rtt,
                         request_in,
                         request_out); });
  } else {
    event_log_->error("{}:{}", ERR_BAD_METHOD, method);
    utils::clear_map_node_put_key_val(request_out, key_error, ERR_BAD_METHOD);
    res = 1;
  }
  return res;
}

int request::on_response(const RestClient::Response &resRC,
                         const int64_t rtt,
                         ryml::NodeRef request_in,
                         ryml::NodeRef request_out)
{
  int res = 0;
  std::ostringstream os;
  os << resRC.code;

  // update conv stats
  parent_.stats_.incr_categorization(os.str());

  // update scenario stats
  parent_.parent_.stats_.incr_categorization(os.str());

  ryml::NodeRef response_in;
  if(request_in.has_child(key_response)) {
    response_in = request_in[key_response];
  }
  ryml::NodeRef response_out = request_out[key_response];
  response_out |= ryml::MAP;

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
                              const std::optional<std::string> &query_string,
                              const std::optional<std::string> &data,
                              std::string &uri_out,
                              RestClient::HeaderFields &reqHF)
{
  if(auth == AUTH_AWS_V2) {
    parent_.auth_.x_amz_date_ = utils::aws_auth::aws_sign_v2_build_date();
    parent_.auth_.aws_sign_v2_build(method, uri_out, reqHF);
  } else if(auth == AUTH_AWS_V4) {
    parent_.auth_.x_amz_date_ = utils::aws_auth::aws_sign_v4_build_date();
    parent_.auth_.aws_sign_v4_build(method,
                                    uri_out,
                                    query_string,
                                    data,
                                    reqHF);
  }
  conv_conn_->SetHeaders(reqHF);
  dump_hdr(reqHF);

  if(query_string && query_string != "") {
    uri_out += '?';
    uri_out += *query_string;
  }

  return 0;
}

int request::post(RestClient::HeaderFields &reqHF,
                  const std::optional<std::string> &auth,
                  const std::string &uri,
                  const std::optional<std::string> &query_string,
                  const std::optional<std::string> &data,
                  const std::function<int(const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req(HTTP_POST, auth, query_string, data, luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_.valid()) {
    if((res = mocked_to_res(resRC))) {
      return res;
    }
  } else {
    resRC = conv_conn_->post(luri, data ? *data : "");
  }
  std::chrono::duration lrtt = std::chrono::system_clock::now() - t0;

  res = cb(resRC, lrtt.count());
  return res;
}

int request::put(RestClient::HeaderFields &reqHF,
                 const std::optional<std::string> &auth,
                 const std::string &uri,
                 const std::optional<std::string> &query_string,
                 const std::optional<std::string> &data,
                 const std::function<int(const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req(HTTP_PUT, auth, query_string, data, luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_.valid()) {
    if((res = mocked_to_res(resRC))) {
      return res;
    }
  } else {
    resRC = conv_conn_->put(luri, data ? *data : "");
  }
  std::chrono::duration lrtt = std::chrono::system_clock::now() - t0;

  res = cb(resRC, lrtt.count());
  return res;
}

int request::get(RestClient::HeaderFields &reqHF,
                 const std::optional<std::string> &auth,
                 const std::string &uri,
                 const std::optional<std::string> &query_string,
                 const std::function<int(const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req(HTTP_GET, auth, query_string, std::nullopt, luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_.valid()) {
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
                 const std::optional<std::string> &query_string,
                 const std::function<int(const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req(HTTP_DELETE, auth, query_string, std::nullopt, luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_.valid()) {
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
                  const std::optional<std::string> &query_string,
                  const std::function<int(const RestClient::Response &, const int64_t)> &cb)
{
  int res = 0;
  std::string luri("/");
  luri += uri;

  if((res = prepare_http_req(HTTP_HEAD, auth, query_string, std::nullopt, luri, reqHF))) {
    return res;
  }

  RestClient::Response resRC;
  std::chrono::system_clock::time_point t0 = std::chrono::system_clock::now();
  if(response_mock_.valid()) {
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
  std::for_each(hdr.begin(), hdr.end(), [&](const auto &it) {
    event_log_->trace("{}:{}", it.first, it.second);
  });
}

int request::mocked_to_res(RestClient::Response &resRC)
{
  bool further_eval = false;

  // code
  auto code = js_env_.eval_as<int32_t>(response_mock_,
                                       key_code,
                                       std::nullopt,
                                       true,
                                       nullptr,
                                       PROP_EVAL_RGX,
                                       &further_eval);
  if(!code) {
    if(further_eval) {
      code = scen_p_evaluator_.eval_as<int32_t>(response_mock_,
                                                key_code,
                                                scen_out_p_resolv_);
    }
  }
  if(code) {
    resRC.code = *code;
    response_mock_.remove_child(key_code);
    response_mock_[key_code] << *code;
  }

  further_eval = false;

  // body
  auto body = js_env_.eval_as<std::string>(response_mock_,
                                           key_body,
                                           std::nullopt,
                                           true,
                                           nullptr,
                                           PROP_EVAL_RGX,
                                           &further_eval);
  if(!body) {
    if(further_eval) {
      body = scen_p_evaluator_.eval_as<std::string>(response_mock_,
                                                    key_body,
                                                    scen_out_p_resolv_);
    }
  }
  if(body) {
    resRC.body = *body;
    response_mock_.remove_child(key_body);
    response_mock_[key_body] << *body;
  }

  return 0;
}

}
