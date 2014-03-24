#ifndef NODE_HPP
#define NODE_HPP
#include <map>
#include <list>

#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>

#include <ccpp_dds_dcps.h>

#include <rclcpp/publisher/publisher.hpp>
#include <rclcpp/subscription/subscription.hpp>

#include <rclcpp/client/client.hpp>
#include <rclcpp/service/service.hpp>

#include <genidlcpp/resolver.h>

template <typename ROSResponseType>
void handle_request(const ROSResponseType& request)
{
}

namespace rclcpp
{

using publisher::Publisher;
using publisher::PublisherInterface;
using subscription::Subscription;
using subscription::SubscriptionInterface;

using client::Client;
using service::Service;

namespace node
{

class Node
{
public:
    Node(std::string name);
    ~Node();

    template <typename ROSMsgType>
    typename Publisher<ROSMsgType>::shared_publisher create_publisher(std::string topic_name, size_t queue_size)
    {
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgType DDSMsg_t;
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgType DDSMsg_var;
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgTypeSupportType DDSMsgTypeSupport_t;
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgTypeSupportType_var DDSMsgTypeSupport_var;
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgDataWriterType DDSMsgDataWriter_t;
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgDataWriterType_var DDSMsgDataWriter_var;
        // TODO check return status
        DDS::ReturnCode_t status;

        DDSMsgTypeSupport_t dds_msg_ts;
        // checkHandle(dds_msg_ts.in(), "new DDSMsgTypeSupport");
        char * dds_msg_name = dds_msg_ts.get_type_name();
        status = dds_msg_ts.register_type(this->participant_.in(), dds_msg_name);
        // checkStatus(status, "TypeSupport::register_type");

        DDS::Publisher_var dds_publisher = this->participant_->create_publisher(
            this->default_publisher_qos_, NULL, DDS::STATUS_MASK_NONE);
        // checkHandle(dds_publisher.in(), "DDS::DomainParticipant::create_publisher");

        DDS::Topic_var dds_topic = this->participant_->create_topic(
            topic_name.c_str(), dds_msg_name, this->default_topic_qos_, NULL,
            DDS::STATUS_MASK_NONE
        );
        // checkHandle(dds_topic.in(), "DDS::DomainParticipant::create_topic");

        DDS::DataWriter_var dds_topic_datawriter = dds_publisher->create_datawriter(
            dds_topic.in(), DATAWRITER_QOS_USE_TOPIC_QOS,
            NULL, DDS::STATUS_MASK_NONE);
        // checkHandle(dds_topic_datawriter.in(), "DDS::Publisher::create_datawriter");

        if (this->publishers_.find(topic_name) != this->publishers_.end())
        {
            // TODO Raise, already called for topic
        }
        // boost::shared_ptr<PublisherInterface> publisher(
            // new Publisher<ROSMsgType>(topic_name, queue_size, dds_publisher, dds_topic, dds_topic_datawriter));

        // this->publishers_.inse/rt(std::pair<std::string, boost::shared_ptr<PublisherInterface> >(topic_name, publisher));
        // return *(dynamic_cast<const Publisher<ROSMsgType> *>(this->publishers_.at(topic_name).get()));
        typename Publisher<ROSMsgType>::shared_publisher publisher(new Publisher<ROSMsgType>(topic_name, queue_size, dds_publisher, dds_topic, dds_topic_datawriter));
        return publisher;
    }

    void destroy_publisher(PublisherInterface * publisher);
    void destroy_publisher(std::string topic_name);

    template <typename ROSMsgType>
    boost::shared_ptr< Subscription<ROSMsgType> > create_subscription(std::string topic_name,
                                                 size_t queue_size,
                                                 typename Subscription<ROSMsgType>::CallbackType cb)
    {
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgTypeSupportType DDSMsgTypeSupport_t;
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgDataReaderType DDSMsgDataReader;
        typedef typename dds_impl::DDSTypeResolver<ROSMsgType>::DDSMsgDataReaderType_var DDSMsgDataReader_var;
        // TODO check return status
        DDS::ReturnCode_t status;

        DDSMsgTypeSupport_t dds_msg_ts;
        // checkHandle(dds_msg_ts.in(), "new DDSMsgTypeSupport");
        char * dds_msg_name = dds_msg_ts.get_type_name();
        status = dds_msg_ts.register_type(this->participant_.in(), dds_msg_name);

        DDS::Subscriber_var dds_subscriber = this->participant_->create_subscriber(
            this->default_subscriber_qos_, NULL, DDS::STATUS_MASK_NONE);

        DDS::Topic_var dds_topic = this->participant_->create_topic(
            topic_name.c_str(), dds_msg_name, this->default_topic_qos_, NULL,
            DDS::STATUS_MASK_NONE
        );

        DDS::DataReader_var topic_reader = dds_subscriber->create_datareader(
            dds_topic.in(), DATAREADER_QOS_USE_TOPIC_QOS,
            NULL, DDS::STATUS_MASK_NONE);

        DDSMsgDataReader_var data_reader = DDSMsgDataReader::_narrow(topic_reader.in());

        boost::shared_ptr< Subscription<ROSMsgType> > subscription(new Subscription<ROSMsgType>(data_reader, cb));
        boost::shared_ptr< SubscriptionInterface > subscription_if = boost::dynamic_pointer_cast< Subscription<ROSMsgType> >(subscription);
        this->subscriptions_.push_back(subscription_if);
        return subscription;
    };

    template <typename ROSRequestType, typename ROSResponseType>
    typename Service<ROSRequestType, ROSResponseType>::shared_service create_service(const std::string &service_name, typename Service<ROSRequestType, ROSResponseType>::CallbackType cb)
    {
        // TODO make a client-specific response queue
        typename Publisher<ROSResponseType>::shared_publisher publisher(this->create_publisher<ROSResponseType>(service_name + "_response", 0));

        typename Service<ROSRequestType, ROSResponseType>::shared_service service(new Service<ROSRequestType, ROSResponseType>(service_name, this, cb, publisher));
        // XXX hardcoded queue_size
        typename Subscription<ROSRequestType>::CallbackType f(boost::bind(&Service<ROSRequestType, ROSResponseType>::handle_request, service, _1));

        boost::shared_ptr< Subscription<ROSRequestType> > request_subscription(this->create_subscription<ROSRequestType>(service_name + "_request", 10, f));

        return service;
    }

    template <typename ROSRequestType, typename ROSResponseType>
    typename Client<ROSRequestType, ROSResponseType>::shared_client create_client(const std::string &service_name)
    {
        typename Publisher<ROSRequestType>::shared_publisher publisher(this->create_publisher<ROSRequestType>(service_name + "_request", 0));
        boost::uuids::uuid client_id_uuid = boost::uuids::random_generator()();
        const std::string client_id = boost::lexical_cast<std::string>(client_id_uuid);
        typename Client<ROSRequestType, ROSResponseType>::shared_client client(new Client<ROSRequestType, ROSResponseType>(client_id, publisher));



        // XXX hardcoded queue_size
        typename Subscription<ROSResponseType>::CallbackType f(boost::bind(&Client<ROSRequestType, ROSResponseType>::handle_response, client, _1));

        // TODO make a client-specific response queue
        boost::shared_ptr< Subscription<ROSResponseType> > response_subscription(this->create_subscription<ROSResponseType>(service_name + "_response", 10, f));

        return client;
    }

    template <typename ROSMsgType>
    void destroy_subscription(Subscription<ROSMsgType> subscription);

    void wait();
private:
    std::string name_;
    DDS::DomainParticipantFactory_var dpf_;
    DDS::DomainParticipant_var participant_;
    DDS::TopicQos default_topic_qos_;
    DDS::PublisherQos default_publisher_qos_;
    DDS::SubscriberQos default_subscriber_qos_;

    std::map<std::string, PublisherInterface* > publishers_;
    std::list< boost::shared_ptr<SubscriptionInterface> > subscriptions_;
    boost::thread *subscription_watcher_th;

    void subscription_watcher();
};

}
}
#endif
