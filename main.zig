const std = @import("std");
const gpio = @import("gpio.zig");

const DRY_VALUE: i32 = 650;
const WET_VALUE: i32 = 276;
const WATER_PORT = 5; // GPIO 5 (D5)
const WATERING_TIME: u64 = 120 * 1000 * 1000 * 1000; // 2 minutes in nanoseconds
const MAX_DAILY_WATERING_TIME: u64 = 30 * 60 * 1000 * 1000 * 1000; // 30 minutes in nanoseconds
const WIFI_TIMEOUT: u64 = 30 * 1000 * 1000 * 1000; // 30 seconds in nanoseconds

fn map(x: i32, in_min: i32, in_max: i32, out_min: i32, out_max: i32) i32 {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

fn logMessage(message: []const u8) void {
    std.debug.print("{}\n", .{message});
    // In actual usage, you would also send this over a TCP connection or similar.
}

pub fn main() anyerror!void {
    const gpio_port = try gpio.init(WATER_PORT);
    gpio_port.low();

    var dailyWateringTime: u64 = 0;
    var lastResetTime = std.time.milliTimestamp();

    while (true) {
        if (std.time.milliTimestamp() - lastResetTime > 24 * 60 * 60 * 1000) {
            dailyWateringTime = 0;
            lastResetTime = std.time.milliTimestamp();
        }

        // Simulated sensor value, replace with actual ADC reading.
        var sensorValue: i32 = 500;
        var moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
        moisturePercentage = std.math.clamp(0, moisturePercentage, 100);
        logMessage("Soil Moisture Value: " ++ std.fmt.bufPrint("{d} -> {d}%", .{sensorValue, moisturePercentage}) catch unreachable);

        const withinTimeRange = true; // Assume within time range for simplicity.

        var shouldStop = false;
        var stopReasons = "Stop Reasons: ";

        if (moisturePercentage >= 50) {
            stopReasons = stopReasons ++ "Moisture is above 50%; ";
            shouldStop = true;
        }
        if (dailyWateringTime >= MAX_DAILY_WATERING_TIME) {
            stopReasons = stopReasons ++ "Daily limit reached; ";
            shouldStop = true;
        }
        if (!withinTimeRange) {
            stopReasons = stopReasons ++ "Out of time range; ";
            shouldStop = true;
        }

        if (shouldStop) {
            logMessage(stopReasons ++ "stopping watering...");
            gpio_port.low();
        } else {
            logMessage("Conditions met, starting watering...");
            gpio_port.high();

            const startWateringTime = std.time.milliTimestamp();
            while (std.time.milliTimestamp() - startWateringTime < WATERING_TIME / 1000000) {
                // Simulated sensor value update, replace with actual ADC reading.
                sensorValue = 500;
                moisturePercentage = map(sensorValue, DRY_VALUE, WET_VALUE, 0, 100);
                moisturePercentage = std.math.clamp(0, moisturePercentage, 100);

                if (!(moisturePercentage < 50) || dailyWateringTime >= MAX_DAILY_WATERING_TIME) {
                    logMessage("Moisture exceeds 50% or daily limit reached, stopping watering...");
                    break;
                }

                logMessage("During Watering - Soil Moisture Value: " ++ std.fmt.bufPrint("{d} -> {d}%", .{sensorValue, moisturePercentage}) catch unreachable);

                std.time.sleep(1_000_000_000); // 1 second
            }
            const wateringDuration = std.time.milliTimestamp() - startWateringTime;
            dailyWateringTime += wateringDuration * 1000_000; // Convert to nanoseconds.
            gpio_port.low();
            logMessage("Watering complete.");
        }

        std.time.sleep(10_000_000_000); // 10 seconds
    }
}
