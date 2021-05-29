#include "ImageRegistry.hpp"
#include "ImageCollection.hpp"

static ImageRegistry gImageRegistry;

float ImageRegistry::getProgress(Key key)
{
    return 0.f;
}

ImageRegistry& getGlobalImageRegistry()
{
    return gImageRegistry;
}

struct ImageLoadingOrder {
    std::shared_ptr<const ImageCollection> collection;
    int index;
};

static moodycamel::BlockingConcurrentQueue<ImageLoadingOrder> Q;

moodycamel::ProducerToken getToken()
{
    return moodycamel::ProducerToken(Q);
}

void flushQueue(const moodycamel::ProducerToken& token)
{
    ImageLoadingOrder toremove;
    while (Q.try_dequeue_from_producer(token, toremove)) {
    }
}

std::pair<std::shared_ptr<ImageProvider>, ImageRegistry::Key> popQueue()
{
    ImageLoadingOrder order;
    Q.wait_dequeue(order);

    auto key = order.collection->getKey(order.index);
    auto provider = order.collection->getImageProvider(order.index);
    std::pair<std::shared_ptr<ImageProvider>, ImageRegistry::Key> info(provider, key);
    return info;
}

void pushQueue(const moodycamel::ProducerToken& token, std::shared_ptr<const ImageCollection> collection, int index)
{
    auto& registry = getGlobalImageRegistry();
    auto key = collection->getKey(index);
    if (registry.getStatus(key) == ImageRegistry::UNKNOWN) {
        ImageLoadingOrder order { collection, index };
        Q.enqueue(token, order);
    }
}

void flushAndPushQueue(const moodycamel::ProducerToken& token, std::shared_ptr<const ImageCollection> collection, int index)
{
    auto& registry = getGlobalImageRegistry();
    auto key = collection->getKey(index);
    if (registry.getStatus(key) == ImageRegistry::UNKNOWN) {
        flushQueue(token);

        ImageLoadingOrder order { collection, index };
        Q.enqueue(token, order);
    }
}
