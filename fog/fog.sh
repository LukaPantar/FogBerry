#!/bin/bash

# MQTT settings
MQTT_BROKER="localhost"
MQTT_PORT=1883
TOPIC="#"  # Subscribe to all topics

# Temporary directory for storing rolling measurement lists
TMP_DIR="/tmp/mqtt_aggregation"
mkdir -p "$TMP_DIR"

# Function to add value and compute/send average if 5 values exist
process_message() {
  local topic="$1"
  local value="$2"

  # Remove leading slash and replace slashes with underscores for filename
  local topic_key="${topic#/}"  # Remove leading slash
  local key_file="$TMP_DIR/$(echo "$topic_key" | tr '/' '_')"

  # Append value to file
  echo "$value" >> "$key_file"

  # Keep only the last 5 values
  tail -n 5 "$key_file" > "$key_file.tmp" && mv "$key_file.tmp" "$key_file"

  # Count how many values we have
  count=$(wc -l < "$key_file")

  # If we have 5, compute average and send
  if [[ "$count" -eq 5 ]]; then
    avg=$(awk '{sum+=$1} END {if (NR>0) print sum/NR}' "$key_file")

    # Extract topic parts
    IFS='/' read -r _ device_id sensor sensor_measurement <<< "$topic"

    # Timestamp
    timestamp=$(date +%s)

    # Construct JSON
    json=$(cat <<EOF
{
  "entries": [{
    "controller": "$device_id",
    "timestamp": $timestamp,
    "$sensor": {
      "_type": "$sensor_measurement",
      "$sensor_measurement": $avg
    }
  }]
}
EOF
)

    # Send POST request
    curl -s -X POST -H "Content-Type: application/json" \
      -d "$json" https://bso.moj-plac.si/api/v1/submit

    echo "Sent average for $topic: $avg"

    # Clear the file after sending
    > "$key_file"
  fi
}

# Listen and process MQTT messages
mosquitto_sub -v -h "$MQTT_BROKER" -p "$MQTT_PORT" -t "$TOPIC" | while read -r topic value; do
  # Check topic format: device/sensor/measurement
  if [[ "$topic" =~ ^/[^/]+/[^/]+/[^/]+$ && "$value" =~ ^[0-9.]+$ ]]; then
    process_message "$topic" "$value"
  else
    echo "Unknown topic: $topic"
  fi
done
