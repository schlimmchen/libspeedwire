#ifndef __PRODUCER_HPP__
#define __PRODUCER_HPP__

#include <Measurement.hpp>


/**
 *  Interface to be implemented by any producer like InfluxDBProducer.
 */
class Producer {
public:
    /** Virtual destructor. */
    virtual ~Producer(void) {}

    /** Flush any internally cached data to the next stage in the processing pipeline. */
    virtual void flush(void) = 0;

    /**
     * Callback to produce the given data to the next stage in the processing pipeline.
     * @param device A string representing the originating device for the data
     * @param type The measurement type of the given data
     * @param wire The Wire enumeration value
     * @param value The data value itself
     */
    virtual void produce(const std::string &device, const MeasurementType &type, const Wire wire, const double value) = 0;
};

#endif
