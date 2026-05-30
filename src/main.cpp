#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

int main(int argc, char *argv[]) {
  if (argc < 3 || std::string(argv[1]) != "-p") {
    std::cerr << "Expected first argument to be '-p'" << std::endl;
    return 1;
  }

  std::string prompt = argv[2];

  if (prompt.empty()) {
    std::cerr << "Prompt must not be empty" << std::endl;
    return 1;
  }

  json messages = json::array({{{"role", "user"}, {"content", prompt}}});

  const char *api_key_env = std::getenv("OPENROUTER_API_KEY");
  const char *base_url_env = std::getenv("OPENROUTER_BASE_URL");

  std::string api_key = api_key_env ? api_key_env : "";
  std::string base_url =
      base_url_env ? base_url_env : "https://openrouter.ai/api/v1";

  if (api_key.empty()) {
    std::cerr << "OPENROUTER_API_KEY is not set" << std::endl;
    return 1;
  }

  while (true) {
    json request_body = {
        {"model", "anthropic/claude-haiku-4.5"},
        {"messages", messages},
        {"tools",
         json::array(
             {{{"type", "function"},
               {"function",
                {{"name", "Read"},
                 {"description", "Read and return the contents of a file"},
                 {"parameters",
                  {{"type", "object"},
                   {"properties",
                    {{"file_path",
                      {{"type", "string"},
                       {"description", "The path to the file to read"}}}}},
                   {"required", json::array({"file_path"})}}}}}}})}};

    cpr::Response response =
        cpr::Post(cpr::Url{base_url + "/chat/completions"},
                  cpr::Header{{"Authorization", "Bearer " + api_key},
                              {"Content-Type", "application/json"}},
                  cpr::Body{request_body.dump()});

    if (response.status_code != 200) {
      std::cerr << "HTTP error: " << response.status_code << std::endl;
      return 1;
    }

    json result = json::parse(response.text);

    if (!result.contains("choices") || result["choices"].empty()) {
      std::cerr << "No choices in response" << std::endl;
      return 1;
    }

    // Ectract the tool call
    auto &message = result["choices"][0]["message"];

    messages.push_back(message);

    if (!message.contains("tool_calls") && message["tool_calls"].empty()) {
      std::cout
          << result["choices"][0]["message"]["content"].get<std::string>();
      break;
    } else {
      for (auto &tool_calls : messages["tool_calls"]) {
        std::string func_name =
            tool_calls["function"]["name"].get<std::string>();

        if (func_name == "Read") {
          json arguments = json::parse(
              tool_calls["function"]["arguments"].get<std::string>());

          std::string file_path = arguments["file_path"].get<std::string>();

          std::filesystem::path path(file_path);

          if (!std::filesystem::exists(path)) {
            throw std::runtime_error("File does not exist: " + file_path);
          }

          if (!std::filesystem::is_regular_file(path)) {
            throw std::runtime_error("Path is not a regular file: " +
                                     file_path);
          }

          std::ifstream file(path, std::ios::in | std::ios::binary);

          if (!file) {
            throw std::runtime_error("Failed to open file: " + file_path);
          }

          std::string file_contents((std::istreambuf_iterator<char>(file)),
                                    std::istreambuf_iterator<char>());

          // std::cout << file_contents << '\n';
        }

        messages.push_back(
            {{"role", "tool"},
             {"tool_call_id", tool_calls["id"].get<std::string>()},
             {"content", message["content"].get<std::string>()}});
      }
    }
  }

  // Parse the function name

  // Parse the arguments

  // Execute the tool

  // You can use print statements as follows for debugging, they'll be
  // visible when running tests.
  std::cerr << "Logs from your program will appear here!" << std::endl;

  // TODO: Uncomment the line below to pass the first stage

  return 0;
}
