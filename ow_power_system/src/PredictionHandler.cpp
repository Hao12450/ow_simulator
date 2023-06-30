// The Notices and Disclaimers for Ocean Worlds Autonomy Testbed for Exploration
// Research and Simulation can be found in README.md in the root directory of
// this repository.

// See PredictionHandler.h for a summary of the purpose of this file.

#include <chrono>
#include <ros/package.h>
#include <ros/ros.h>
#include <sensor_msgs/JointState.h>
#include <PrognoserFactory.h>

#include "PredictionHandler.h"

using namespace PCOE;

// The indices use to access temperature information.
// This might change to median SOC or RUL index or fixed percentile.
// (predecessor comment to Summer 2022, unsure exactly what it means)
static constexpr int BATTERY_TEMPERATURE_INDEX = 0;
static constexpr int MODEL_TEMPERATURE_INDEX = 1;

/**
 * Constructs a new prediction handler that subscribes to battery EoD
 * predictions for the specified source and on the specified message
 * bus.
 **/
PredictionHandler::PredictionHandler(MessageBus& bus, const std::string& src) :
                                     m_bus(bus)
{
  m_bus.subscribe(this, src, MessageId::BatteryEod);
  m_event_ready = false;
}

/**
 * Unsubscribes the prediction handler from the message bus.
 **/
PredictionHandler::~PredictionHandler()
{
  m_bus.unsubscribe(this);
}

/**
 * The message bus will call this function each time the predictor publishes
 * a new battery EoD prediction.
 **/
void PredictionHandler::processMessage(const std::shared_ptr<Message>& message)
{
  using namespace std::chrono;
  // The prediction handler only ever subscribes to the BatteryEoD message
  // id, which should always be a ProgEventMessage, so this should always
  // succeed.
  auto prediction_msg = dynamic_cast<ProgEventMessage*>(message.get());
  if (prediction_msg == nullptr)
  {
    ROS_ERROR("Failed to cast prediction message to expected type");
    return;
  }

  // Get the event for battery EoD
  auto eod_event = prediction_msg->getValue();

  // The time of event is a `UData` structure, which represents a data
  // point while maintaining uncertainty. For the MonteCarlo predictor
  // used by this example, the uncertainty is captured by storing the
  // result of each particle used in the prediction.
  UData eod_time = eod_event.getTOE();
  if (eod_time.uncertainty() != UType::Samples)
  {
    ROS_ERROR("Unexpected uncertainty type for EoD prediction");
    return;
  }

  // Get the median RUL, SoC, and battery temperature from the returned
  // prediction:

  // valid prediction
  // Determine the median RUL.
  double eod_median = findMedian(eod_time.getVec());
  auto now = MessageClock::now();
  auto now_s = duration_cast<std::chrono::seconds>(now.time_since_epoch());
  double rul_median = eod_median - now_s.count();

  // Determine the median SOC.
  UData currentSOC = eod_event.getState()[0];
  double soc_median = findMedian(currentSOC.getVec());

  // Determine the Battery Temperature
  auto stateSamples = eod_event.getSystemState()[0];
  std::vector<double> temperature_state;
  for (auto sample : stateSamples)
  {
    temperature_state.push_back(sample[BATTERY_TEMPERATURE_INDEX]);
  }

  // HACK ALERT (as of Sept '22):
  // GSAP's asynchronous prognoser does not have a function to get its model like
  // the simple prognoser does. As such, one can instantiate a simple prognoser
  // and get its model instead; it should be identical. However, a getter function
  // would be much more ideal and more efficient.

  // Construct a simple prognoser to get the battery model.
  auto prognoser_config_path = ros::package::getPath("ow_power_system") + "/config/prognoser.cfg";
  ConfigMap prognoser_config(prognoser_config_path);
  std::unique_ptr<PCOE::Prognoser> temp_prog =
    PrognoserFactory::instance().Create("ModelBasedPrognoser", prognoser_config);

  auto& model = static_cast<ModelBasedPrognoser*>(temp_prog.get())->getModel();

  auto model_output = model.outputEqn(now_s.count(), static_cast<PrognosticsModel::state_type>(temperature_state));

  // Put the newly obtained data in the stored references.
  m_curr_EoD.remaining_useful_life = rul_median;
  m_curr_EoD.state_of_charge = soc_median;
  m_curr_EoD.battery_temperature = model_output[MODEL_TEMPERATURE_INDEX];

  // A new event is ready.
  m_event_ready = true;
}

/**
 * Returns the EoDValues struct currently stored and
 * updates the bool.
**/
EoDValues PredictionHandler::getEoD()
{
  m_event_ready = false;
  return m_curr_EoD;
}

/**
 * Returns the bool representing the status of the bus
 * (i.e. if it has a new EoD prediction ready to return).
**/
bool PredictionHandler::getStatus()
{
  return m_event_ready;
}

double PredictionHandler::findMedian(std::vector<double> samples)
{
  std::nth_element(samples.begin(), (samples.begin() + (samples.size() / 2)),
                   samples.end());
  return samples.at(samples.size() / 2);
}