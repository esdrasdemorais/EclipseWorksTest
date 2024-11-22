#include <stdlib.h>
#include <glib.h>
#include <librdkafka/rdkafka.h>
#include <nlohmann/json.hpp>
#include <nlohmann/adl_serializer.hpp>
#include <set>
#include <iostream>
#include <memory>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <curl/curl.h>
//#include "base64.h"
//#include "base64.cpp"
#include <boost/shared_ptr.hpp>		  // ServiceLocator Dependency Injection
#include <boost/uuid/uuid.hpp>            // uuid class
#include <boost/uuid/uuid_generators.hpp> // generators
#include <boost/uuid/uuid_io.hpp>         // streaming operators etc.

static void load_config_group(rd_kafka_conf_t *conf,
                              GKeyFile *key_file,
                              const char *group
                              ) {
    char errstr[512];
    g_autoptr(GError) error = NULL;

    gchar **ptr = g_key_file_get_keys(key_file, group, NULL, &error);
    if (error) {
        g_error("%s", error->message);
        exit(1);
    }

    while (*ptr) {
        const char *key = *ptr;
        g_autofree gchar *value = g_key_file_get_string(key_file, group, key, &error);

        if (error) {
            g_error("Reading key: %s", error->message);
            exit(1);
        }

        if (rd_kafka_conf_set(conf, key, value, errstr, sizeof(errstr))
            != RD_KAFKA_CONF_OK
            ) {
            g_error("%s", errstr);
            exit(1);
        }

        ptr++;
    }
}

/* Optional per-message delivery callback (triggered by poll() or flush())
 * when a message has been successfully delivered or permanently
 * failed delivery (after retries).
 */
static void dr_msg_cb (rd_kafka_t *kafka_handle,
                       const rd_kafka_message_t *rkmessage,
                       void *opaque) {
    if (rkmessage->err) {
        g_error("Message delivery failed: %s", rd_kafka_err2str(rkmessage->err));
    }
}

using json = nlohmann::json;

namespace eclipseworkstest {
    struct User {
    	std::string id;
        std::string email;
        std::string name;

        User() {}
    };
}

namespace eclipseworkstest {	
    void to_json(json& j, const User& u) {
        j = json{ {"id", u.id}, {"email", u.email}, {"name", u.name} };
    }
}

namespace eclipseworkstest {
    struct Task {
	std::string id;
        std::string title;
	std::string description;
	std::string dueDate;
	std::string state;
	bool priority;

    Task() {}
    };
}

namespace eclipseworkstest {
    void to_json(json& j, const Task& t) {
	    j = json{ {"id", t.id}, {"title", t.title}, {"description", t.description}, {"dueDate", t.dueDate}, {"state", t.state}, {"priority", t.priority} };
    }
}

namespace eclipseworkstest {
    struct Project {
	std::string id;
	std::string name;
	std::vector<Task> taskList;
    User user;

    Project() {}
    };
}

namespace eclipseworkstest {
    void to_json(json& j, const Project& p) {
	    j += json{ {"id", p.id}, {"name", p.name}, {"taskList", p.taskList} };
    }
}

#include "ServiceLocator.hpp"

#define SERVICELOCATOR_SPTR

/*
template <class T>
using sptr = boost::shared_ptr<T>;

template <class T>
using const_sptr = boost::shared_ptr<const T>;

template <class T>
using wptr = boost::weak_ptr<T>;

template <class T>
using uptr = boost::unique_ptr<T>;
*/

namespace eclipseworkstest {
    static volatile sig_atomic_t run = 1;

    /**
     * @brief Signal termination of program
     */
    static void stop(int sig) {
        run = 0;
    }

    class RdKafkaConf {
	  private:
        const char* _config_file = "./config.ini";
      private:
	    rd_kafka_conf_t* _conf;
	  private:
        rd_kafka_conf_t* loadFile(const char* config_file) {
            g_autoptr(GError) error = NULL;
            g_autoptr(GKeyFile) key_file = g_key_file_new();
            if (!g_key_file_load_from_file(key_file, config_file, G_KEY_FILE_NONE, &error)) {
                g_error("Error loading config file: %s", error->message);
            }

            // Load the relevant configuration sections.
            load_config_group(_conf, key_file, "default");

            // Install a delivery-error callback.
            rd_kafka_conf_set_dr_msg_cb(_conf, dr_msg_cb);

            _conf = rd_kafka_conf_new();
            load_config_group(_conf, key_file, "default");
            load_config_group(_conf, key_file, "consumer");

            return _conf;
        }
      public:
	    explicit RdKafkaConf() {
            _conf = loadFile(_config_file);       
        }
	  public:
	    rd_kafka_conf_t* getConf() {
	    	return _conf;
	    }
    };

    class KafkaConfig {
	private:
	    sptr<RdKafkaConf> _rdConf;
	public:
	    explicit KafkaConfig(sptr<RdKafkaConf> rdConf) : _rdConf(rdConf) { 
	    }
    public:
        rd_kafka_conf_t* getConf() {
            return _rdConf->getConf();
        }
    };

    template <class E>
    class IKafka {
    private:
        sptr<KafkaConfig> _config;
	    sptr<E> _entity;
    public:
       	virtual bool create(E entity) {
            rd_kafka_t *producer;
            rd_kafka_conf_t *conf = _config->getConf();
            char errstr[512];

            // Create the Producer instance.
            producer = rd_kafka_new(RD_KAFKA_PRODUCER, conf, errstr, sizeof(errstr));
            if (!producer) {
                g_error("Failed to create new producer: %s", errstr);
                return 1;
            }

            // Configuration object is now owned, and freed, by the rd_kafka_t instance.
            conf = NULL;

            // Produce data by selecting random values from these lists.
            //int message_count = 1;

	        const char *topic = typeid(_entity).name();

//            const char *keys[8] = {"video", "photo", "title", "description", "value", "km", "link", "\0"};
//            const char *values[8] = {"", "", "Chevrolet/ CRUZE LTZ 1.4 2018 (Top de linha)", "ÚNICO DONO \nR$98.000,00\n59.000km\n\nBanco revestido em couro\nDireção Assistida\nMultimídia\nAr Condicionado\nSensores de estacionamento traseiro\nFaróis com acendimento automático\nComputador de bordo\nAr condicionado\nFarol de neblina", "R$98.000,00", "59.000km", "https://www.instagram.com/garciasmotors/", "\0"};

            //printf("ARR_SIZE=%ld", ARR_SIZE(keys));

            const char *key = "";
            const char *value = "";
	       
            rd_kafka_resp_err_t err;

            //for (int i = 0; i < message_count; i++) {
                //strcpy(key, keys);
                //strncpy(value, values, ARR_SIZE(values));
                boost::uuids::random_generator generator;
                boost::uuids::uuid guid = generator();
                //boost::uuids::uuid guid = guid();

                json jsonEntity = json::parse(entity);
                //to_json(jsonEntity, entity);
    
                size_t key_len = strlen(jsonEntity.dump().c_str());
                size_t value_len = strlen(jsonEntity.dump().c_str());

                key = boost::uuids::to_string(guid).c_str();
                key_len = strlen(key);

                std::cout << jsonEntity << std::endl;

                std::string jsonString = jsonEntity.dump();
           
                value_len = strlen(jsonString.c_str());

                err = rd_kafka_producev(producer,
                        RD_KAFKA_V_TOPIC(topic),
                        RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
                    //    RD_KAFKA_V_KEY((void*)key, key_len),
                    //    RD_KAFKA_V_VALUE((void*)jsonString.c_str(), value_len),
                        RD_KAFKA_V_OPAQUE(NULL),
                        RD_KAFKA_V_END);

                if (err) {
                    g_error("Failed to produce to topic %s: %s", topic, rd_kafka_err2str(err));
                    return 1;
                } else {
                    g_message("Produced event to topic %s: key = %12s value = %12s", topic, key, value);
                }

                rd_kafka_poll(producer, 0);
            //}

            // Block until the messages are all sent.
            g_message("Flushing final messages..");
            rd_kafka_flush(producer, 5 * 1000);

            if (rd_kafka_outq_len(producer) > 0) {
                g_error("%d message(s) were not delivered", rd_kafka_outq_len(producer));
            }

            g_message("%d events were produced to topic %s.", 1, topic);

            rd_kafka_destroy(producer);

            return true;
    	}
    public:
	void setEntity(sptr<E> entity) {
	    _entity = entity;        
    }
    public:
    void setKafkaConfig(sptr<KafkaConfig> config) {
        _config = config;
    }
    private:
    void setJsonEntity(json jsonEntity, E entity) {
	    entity.name = jsonEntity["name"].get<std::string>().c_str();
	    //entity->photo = jsonLead["photo"].get<std::string>().c_str();
	    //entity->title = jsonLead["title"].get<std::string>().c_str();
	    //entity->description = jsonLead["description"].get<std::string>().c_str();
    }
    private:
	virtual std::vector<E> consumerPolling(rd_kafka_t *consumer, rd_kafka_topic_partition_list_t *subscription) {
	    std::vector<E> response;

	    rd_kafka_topic_partition_list_destroy(subscription);

	    // Install a signal handler for clean shutdown.
	    signal(SIGINT, stop);

	    // Start polling for messages.
	    while (run) {
            rd_kafka_message_t *consumer_message;

            consumer_message = rd_kafka_consumer_poll(consumer, 500);
            if (!consumer_message) {
                g_message("Waiting...");
                continue;
            }

            if (consumer_message->err) {
                if (consumer_message->err == RD_KAFKA_RESP_ERR__PARTITION_EOF) {
                /* We can ignore this error - it just means we've read
                 * everything and are waiting for more data.
                 */
                } else {
                    g_message("Consumer error: %s", rd_kafka_message_errstr(consumer_message));

                    /*sentry_capture_event(sentry_value_new_message_event(
                      / *   level * / SENTRY_LEVEL_ERROR,
                      / *  logger * / "custom",
                      / * message * / rd_kafka_message_errstr(consumer_message)
                    ));*/

                    break;
                }
            } else {
                g_message("Consumed event from topic %s: key = %.*s value = %s",
                      rd_kafka_topic_name(consumer_message->rkt),
                      (int)consumer_message->key_len,
                      (char *)consumer_message->key,
                      (char *)consumer_message->payload
                      );

                char *jsonString = (char *)consumer_message->payload;

                json jsonEntity = json::parse(jsonString);
                
                E entity;
                setJsonEntity(jsonEntity, entity);

                response.push_back(entity);
            }

            rd_kafka_message_destroy(consumer_message);
	    }

	    g_message("Closing consumer");
	    rd_kafka_consumer_close(consumer);

	    rd_kafka_destroy(consumer);

	    return response;
	}
    public:
	virtual std::vector<E> read() {
std::cout << "Aqui chegou!" << std::endl;

        rd_kafka_t *consumer;
	    rd_kafka_conf_t *conf = _config->getConf();

        std::cout << "Aqui agora: " << conf << std::endl;

	    rd_kafka_resp_err_t err;
	    char errstr[512];
	    std::vector<E> response;

	    consumer = rd_kafka_new(RD_KAFKA_CONSUMER, conf, errstr, sizeof(errstr));
	    if (!consumer) {
		    g_error("Failed to create new consumer: %s", errstr);
	    }
	    rd_kafka_poll_set_consumer(consumer);

	    // Configuration object is now owned, and freed, by the rd_kafka_t instance.
	    conf = NULL;

	    const char *topic = typeid(_entity).name();

	    std::cout << "Topic Name:" << topic << std::endl;

        rd_kafka_topic_partition_list_t *subscription = rd_kafka_topic_partition_list_new(1);
	    rd_kafka_topic_partition_list_add(subscription, topic, RD_KAFKA_PARTITION_UA);

	    err = rd_kafka_subscribe(consumer, subscription);
	    if (err) {
            g_error("Failed to subscribe to %d topics: %s", subscription->cnt, rd_kafka_err2str(err));
            rd_kafka_topic_partition_list_destroy(subscription);
            rd_kafka_destroy(consumer);
	    }
	    response = consumerPolling(consumer, subscription);
	    return response;
	}
    public:
	virtual bool update() {
	    return true;
	}
    public:
	virtual bool delet() {
	    return true;
	}
    };

    class KafkaInfraProject : public IKafka<Project> {
    };

//    class KafkaInfraTask : IKafka<Task> {
    //protected:
//	sptr<IKafka<Task>> *_kafka;
    //public:
//	KafkaInfraTask(sptr<IKafka<Task>> *kafka) : _kafka(kafka) {}
  //  };

}

namespace eclipseworkstest {
    template <class E>
    class IRepository {
    private:
	    sptr<E> _entity;
    protected:
        sptr<IKafka<E>> _kafka;
    public:
    void setEntity(sptr<E> entity) {
        _entity = entity;
        std::cout << "entity = " << _entity << std::endl;
    }
    /*public:
    virtual void setKafkaInfra(sptr<K> kafka) {
        _kafka = kafka;
    }*/
    public:
	virtual bool create(E entity) {
	    return _kafka->create(entity);
	}
	virtual std::vector<E> read() {
	    return _kafka->read();
	}
	virtual bool update() {
	    return true;// _kafka->update(_entity);
	}
	virtual bool delet() {
	    return true;// _kafka->delet(_entity);
	}
    };

    class ProjectRepository : public IRepository<Project> {
    public:
	void setKafkaInfra(sptr<IKafka<Project>> kafka) {
        _kafka = kafka;
    }
    };

    //class TaskRepository : IRepository<Task, KafkaInfraTask> {
	//private:
	//    sptr<IRepository<Task, KafkaInfraTask>> *_repository;
	//public:
	  //  TaskRepository() : _entity(entity), _kafka(kafka) {}
    //};
}

namespace eclipseworkstest {
    /* The SLModule classes are ServiceLocator aware, and they are also intimate with the concrete classes they bind to
       and so know what dependancies are required to create instances */
    class RdKafkaConfSLModule : public ServiceLocator::Module {
      public:
      void load() override {
        bind<RdKafkaConf>("RdKafkaConf").to<RdKafkaConf>([](SLContext_sptr slc) {
            return new RdKafkaConf();
        }).asSingleton();
      }
    };

    class KafkaConfigSLModule : public ServiceLocator::Module {
      public:
      void load() override {
        bind<KafkaConfig>("KafkaConfig").to<KafkaConfig>([](SLContext_sptr slc) {
	      return new KafkaConfig(slc->resolve<RdKafkaConf>("RdKafkaConf"));
	    }).asSingleton();
      }
    };

    class ProjectSLModule : public ServiceLocator::Module {
      public:
      void load() override {
        bind<Project>("Project").to<Project>([](SLContext_sptr slc) {
            return new Project();
        }).asTransient();
      }
    };

    class KafkaInfraSLModule : public ServiceLocator::Module {
      public:
      void load() override {
	    bind<IKafka<Project>>("KafkaInfraProject").to<KafkaInfraProject>([](SLContext_sptr slc) {
            sptr<Project> project = slc->resolve<Project>("Project");
            std::cout << "project in KafkaInfraProject = " << project << std::endl;
            KafkaInfraProject* kafkaInfraProject = new KafkaInfraProject();
            auto kafkaConfig = slc->resolve<KafkaConfig>("KafkaConfig");
            slc->afterResolve([kafkaInfraProject, project, kafkaConfig](SLContext_sptr slc) {
                kafkaInfraProject->setKafkaConfig(kafkaConfig);
                kafkaInfraProject->setEntity(project);
            });
            return kafkaInfraProject;
	    }).asTransient();
	/*bind<IKafka>("KafkaInfraTask").to<KafkaInfraTask>([] (SLContext_sptr slc) { 
	    return new KafkaInfraTask();
	});*/
      }
    };

    class RepositorySLModule : public ServiceLocator::Module {
    public:
      void load() override {
	    bind<IRepository<Project>>("ProjectRepository").to<ProjectRepository>([](SLContext_sptr slc) {
	        sptr<Project> project = slc->resolve<Project>("Project");
            std::cout << "RepositorySLModule: " << project << std::endl;
	        auto kafkaInfraProject = slc->resolve<IKafka<Project>>("KafkaInfraProject");
            std::cout << "kafkaInfraProject = " << kafkaInfraProject << std::endl;
            ProjectRepository* projectRepository = new ProjectRepository();
            std::cout << "projectRepository RepositorySLModule -> " << projectRepository << std::endl;
            slc->afterResolve([project, kafkaInfraProject, projectRepository](SLContext_sptr slc) {
                //projectRepository = new ProjectRepository(project, kafkaInfraProject);
                projectRepository->setEntity(project);
                projectRepository->setKafkaInfra(kafkaInfraProject);  
            });
            std::cout << projectRepository << std::endl;
            return projectRepository;
	    }).asTransient();
  /*bind<eclipseworkstest::IRepository>("TaskRepository").to<TaskRepository>([] (SLContext_sptr slc) { 
      return new TaskRepository();
  });*/
      }
    };
}
