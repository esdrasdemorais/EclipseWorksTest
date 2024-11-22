#include <stdio.h>
#include <iostream>
#include "common.cpp"
#include "crow_all.h"
#include <glib.h>
#include <string.h>

#define CROW_JSON_USE_MAP
#define CROW_MAIN
#define CROW_DEFLATE
#define CROW_COMPRESSION

using json = nlohmann::json;

using Task = eclipseworkstest::Task;
using Project = eclipseworkstest::Project;
using User = eclipseworkstest::User;
using KafkaConfig = eclipseworkstest::KafkaConfig;
using KafkaInfraProject = eclipseworkstest::KafkaInfraProject;
using RdKafkaConfSLModule = eclipseworkstest::RdKafkaConfSLModule;
using KafkaConfigSLModule = eclipseworkstest::KafkaConfigSLModule;
using KafkaInfraSLModule = eclipseworkstest::KafkaInfraSLModule;
using RepositorySLModule = eclipseworkstest::RepositorySLModule;
using ProjectSLModule = eclipseworkstest::ProjectSLModule;
using ProjectRepository = eclipseworkstest::ProjectRepository;

int main(int argc, char **argv) {
    if (argc != 2) {
	    g_error("Usage: %s <config.ini>", argv[0]);
	    return 1;
    }

    // See https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md
    char *config_file = argv[1];

    std::cout << "config_file = " << config_file << std::endl;

    //KafkaConfig* kafkaConfig(NULL);
    //rd_kafka_conf_t *kafkaConf = kafkaConfig->loadFile(config_file);

    auto sl = ServiceLocator::create();

    sl->modules()
        .add<RdKafkaConfSLModule>()
	    .add<KafkaConfigSLModule>()
	    .add<ProjectSLModule>()
        .add<KafkaInfraSLModule>()
        .add<RepositorySLModule>();

    auto slc = sl->getContext();
    
    std::cout << "slc = " << slc << std::endl;
auto project = slc->resolve<Project>("Project");
std::cout << "project = " << project << std::endl;
    
    auto kafkaInfraProject = slc->resolve<eclipseworkstest::IKafka<Project>>("KafkaInfraProject");
std::cout << "kafkaInfraProject = " << kafkaInfraProject << std::endl;

    auto projectRepository = slc->resolve<eclipseworkstest::IRepository<Project>>("ProjectRepository");
    
    //std::vector<sptr<eclipseworkstest::IRepository<Project, KafkaInfraProject>>> projectRepository;
    //slc->resolveAll<eclipseworkstest::IRepository<Project, KafkaInfraProject>>(&projectRepository);
   
    std::cout << "projectRepository = " << projectRepository << std::endl;

    std::vector<Project> projects = projectRepository->read();

    std::cout << "projects.size = " << projects.size() << std::endl;

    crow::SimpleApp app;

    CROW_ROUTE(app, "/project/list/<string>")([projects](std::string user){
        json j;
        for (auto project : projects) {
            eclipseworkstest::to_json(j, project); 
        }
        //crow::json::wvalue x({{"user",user},{"projectsCount",""+projects.size()},{"projects",j}});
        return j.dump();
        //"list projects per <user>";
    });

    CROW_ROUTE(app, "/task/list/<string>")([](std::string project){
	    crow::json::wvalue x({{"message",""}});
	    return "list tasks per <project>";
    });

    CROW_ROUTE(app, "/project")([projectRepository](const crow::request& request){
	    auto body = crow::json::load(request.body);
        
        if (!body) return crow::response(400);
        
        Task t1 = { "1", "t1", "t1", "2023-12-07", "em andamento" };
        Task t2 = { "2", "t2", "t2", "2023-12-07", "em andamento" };

        std::vector<Task> tasks { t1, t2 };

        Project project = { "1", "p1", tasks };

        json j = project;
            
        std::cout << j << std::endl;

        std::ostringstream os;
        os << body;
        //return crow::response(os.str());

        auto response = projectRepository->create(project);

        return crow::response(response);
    });

    /*CROW_ROUTE(app, "/task")([](const crow::request& request){
	auto body =  crow::json::load(request.body);
	if (!body) return crow::response(400);
	std::ostringstream os;
	os << body;
	return crow::response(os.str());
    });

    CROW_ROUTE(app, "/task/update")([](const crow::request& request){
	auto body = crow::json::load(request.body);
	if (!body) return crow::response(400);
	std::ostringstream os;
	os << body;
	return crow::response(os.str());
    });

    CROW_ROUTE(app, "/task/remove")([](const crow::request& request){
	auto body = crow::json::load(request.body);
	if (!body) return crow::response(400);
	std::ostringstream os;
	os << body;
	return crow::response(os.str());
    });*/

    app.loglevel(crow::LogLevel::Debug);

    app.port(18080)
       .multithreaded()
       .run();

    return 0;
}
