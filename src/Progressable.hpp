#pragma once

class Progressable {
public:
    virtual float getProgressPercentage() const = 0;
    virtual bool isLoaded() const = 0;
    virtual void progress() = 0;
    virtual ~Progressable() = default;
};

