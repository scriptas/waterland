use embedded_svc::wifi::{ClientConfiguration, Configuration, Wifi};
use esp_idf_hal::gpio::{PinDriver, Output};
use esp_idf_hal::prelude::*;
use esp_idf_svc::eventloop::EspSystemEventLoop;
use esp_idf_svc::netif::EspNetifStack;
use esp_idf_svc::nvs::EspDefaultNvs;
use esp_idf_svc::sysloop::EspSysLoopStack;
use esp_idf_svc::wifi::EspWifi;
use esp_idf_svc::sntp::{self, SyncStatus};
use esp_idf_sys as _;
use std::sync::Arc;use embedded_svc::wifi::{ClientConfiguration, Configuration, Wifi};
use esp_idf_hal::gpio::{PinDriver, Output};
use esp_idf_hal::prelude::*;
use esp_idf_svc::eventloop::EspSystemEventLoop;
use esp_idf_svc::netif::EspNetifStack;
use esp_idf_svc::nvs::EspDefaultNvs;
use esp_idf_svc::sysloop::EspSysLoopStack;
use esp_idf_svc::wifi::EspWifi;
use esp_idf_svc::sntp::{self, SyncStatus};
use esp_idf_sys as _;
use std::sync::Arc;
use std::time::{Duration, SystemTime};

const SSID: &str = "wifiname";
const PASSWORD: &str = "wifipass";

const DRY_VALUE: i32 = 650;
const WET_VALUE: i32 = 276;
const WATER_PORT: i32 = 5;  // GPIO 5 (D5)
const WATERING_TIME: u64 = 120;  // 2 minutes in seconds
const MAX_DAILY_WATERING_TIME: u64 = 1800;  // 30 minutes in seconds
const WIFI_TIMEOUT: u64 = 30;  // 30 seconds
const TCP_PORT: u16 = 23;  // TCP port for logging

fn main() -> anyhow::Result<()> {
    esp_idf_sys::link_patches();

    let peripherals = Peripherals::take().unwrap();
    let pins = peripherals.pins;

    let mut water_port = PinDriver::output(pins.gpio5)?;

    let sysloop = Arc::new(EspSysLoopStack::new()?);
    let netif = Arc::new(EspNetifStack::new()?);
    let nvs = Arc::new(EspDefaultNvs::new()?);

    let mut wifi = EspWifi::new(netif, sysloop.clone(), Some(nvs))?;

    let wifi_config = Configuration::Client(ClientConfiguration {
        ssid: SSID.into(),
        password: PASSWORD.into(),
        ..Default::default()
    });

    wifi.set_configuration(&wifi_config)?;

    wifi.start()?;
    wifi.connect()?;

    let start = SystemTime::now();
    while SystemTime::now().duration_since(start)? < Duration::from_secs(WIFI_TIMEOUT)
        && !wifi.is_connected()?
    {
        std::thread::sleep(Duration::from_millis(500));
    }

    if wifi.is_connected()? {
        println!("WiFi connected.");
        println!("IP address: {:?}", wifi.sta_netif().ip_info()?);

        // Start SNTP service
        sntp::start_sntp_default();
        sntp::set_server_name(0, "pool.ntp.org");
        sntp::set_time_sync_notification_callback(|status| {
            if status == SyncStatus::Completed {
                println!("Time synchronized");
            }
        });

        let mut daily_watering_time = 0u64;
        let mut last_reset_time = SystemTime::now();

        loop {
            if SystemTime::now().duration_since(last_reset_time)? >= Duration::from_secs(86400) {
                daily_watering_time = 0;
                last_reset_time = SystemTime::now();
            }

            // Read sensor value (simulated)
            let sensor_value: i32 = 500;  // Replace with actual sensor reading
            let moisture_percentage = map(sensor_value, DRY_VALUE, WET_VALUE, 0, 100);
            println!(
                "Soil Moisture Value: {} -> {}%",
                sensor_value, moisture_percentage
            );

            // Determine watering conditions
            let current_time = SystemTime::now();
            let within_time_range = if let Ok(duration) = current_time.duration_since(SystemTime::UNIX_EPOCH) {
                let hours = (duration.as_secs() % 86400) / 3600;
                hours >= 10 && hours < 18
            } else {
                true
            };

            let mut should_stop = false;
            let mut stop_reasons = String::from("Stop Reasons: ");

            if moisture_percentage >= 50 {
                stop_reasons.push_str("Moisture is above 50%; ");
                should_stop = true;
            }
            if daily_watering_time >= MAX_DAILY_WATERING_TIME {
                stop_reasons.push_str("Daily limit reached; ");
                should_stop = true;
            }
            if !within_time_range {
                stop_reasons.push_str("Out of time range; ");
                should_stop = true;
            }

            if should_stop {
                println!("{} stopping watering...", stop_reasons);
                water_port.set_low()?;
            } else {
                println!("Conditions met, starting watering...");
                water_port.set_high()?;

                let start_watering_time = SystemTime::now();
                while SystemTime::now().duration_since(start_watering_time)? < Duration::from_secs(WATERING_TIME) {
                    let sensor_value: i32 = 500;  // Replace with actual sensor reading
                    let moisture_percentage = map(sensor_value, DRY_VALUE, WET_VALUE, 0, 100);
                    println!(
                        "During Watering - Soil Moisture Value: {} -> {}%",
                        sensor_value, moisture_percentage
                    );

                    if !(moisture_percentage < 50) || daily_watering_time >= MAX_DAILY_WATERING_TIME {
                        println!("Moisture exceeds 50% or daily limit reached, stopping watering...");
                        break;
                    }

                    std::thread::sleep(Duration::from_secs(1));
                }

                let watering_duration = SystemTime::now().duration_since(start_watering_time)?;
                daily_watering_time += watering_duration.as_secs();
                water_port.set_low()?;
                println!("Watering complete.");
            }

            std::thread::sleep(Duration::from_secs(10));
        }
    } else {
        println!("WiFi connection failed. Continuing without WiFi.");
    }

    Ok(())
}

fn map(x: i32, in_min: i32, in_max: i32, out_min: i32, out_max: i32) -> i32 {
    (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min
}

use std::time::{Duration, SystemTime};

const SSID: &str = "wifiname";
const PASSWORD: &str = "wifipass";

const DRY_VALUE: i32 = 650;
const WET_VALUE: i32 = 276;
const WATER_PORT: i32 = 5;  // GPIO 5 (D5)
const WATERING_TIME: u64 = 120;  // 2 minutes in seconds
const MAX_DAILY_WATERING_TIME: u64 = 1800;  // 30 minutes in seconds
const WIFI_TIMEOUT: u64 = 30;  // 30 seconds
const TCP_PORT: u16 = 23;  // TCP port for logging

fn main() -> anyhow::Result<()> {
    esp_idf_sys::link_patches();

    let peripherals = Peripherals::take().unwrap();
    let pins = peripherals.pins;

    let mut water_port = PinDriver::output(pins.gpio5)?;

    let sysloop = Arc::new(EspSysLoopStack::new()?);
    let netif = Arc::new(EspNetifStack::new()?);
    let nvs = Arc::new(EspDefaultNvs::new()?);

    let mut wifi = EspWifi::new(netif, sysloop.clone(), Some(nvs))?;

    let wifi_config = Configuration::Client(ClientConfiguration {
        ssid: SSID.into(),
        password: PASSWORD.into(),
        ..Default::default()
    });

    wifi.set_configuration(&wifi_config)?;

    wifi.start()?;
    wifi.connect()?;

    let start = SystemTime::now();
    while SystemTime::now().duration_since(start)? < Duration::from_secs(WIFI_TIMEOUT)
        && !wifi.is_connected()?
    {
        std::thread::sleep(Duration::from_millis(500));
    }

    if wifi.is_connected()? {
        println!("WiFi connected.");
        println!("IP address: {:?}", wifi.sta_netif().ip_info()?);

        // Start SNTP service
        sntp::start_sntp_default();
        sntp::set_server_name(0, "pool.ntp.org");
        sntp::set_time_sync_notification_callback(|status| {
            if status == SyncStatus::Completed {
                println!("Time synchronized");
            }
        });

        let mut daily_watering_time = 0u64;
        let mut last_reset_time = SystemTime::now();

        loop {
            if SystemTime::now().duration_since(last_reset_time)? >= Duration::from_secs(86400) {
                daily_watering_time = 0;
                last_reset_time = SystemTime::now();
            }

            // Read sensor value (simulated)
            let sensor_value: i32 = 500;  // Replace with actual sensor reading
            let moisture_percentage = map(sensor_value, DRY_VALUE, WET_VALUE, 0, 100);
            println!(
                "Soil Moisture Value: {} -> {}%",
                sensor_value, moisture_percentage
            );

            // Determine watering conditions
            let current_time = SystemTime::now();
            let within_time_range = if let Ok(duration) = current_time.duration_since(SystemTime::UNIX_EPOCH) {
                let hours = (duration.as_secs() % 86400) / 3600;
                hours >= 10 && hours < 18
            } else {
                true
            };

            let mut should_stop = false;
            let mut stop_reasons = String::from("Stop Reasons: ");

            if moisture_percentage >= 50 {
                stop_reasons.push_str("Moisture is above 50%; ");
                should_stop = true;
            }
            if daily_watering_time >= MAX_DAILY_WATERING_TIME {
                stop_reasons.push_str("Daily limit reached; ");
                should_stop = true;
            }
            if !within_time_range {
                stop_reasons.push_str("Out of time range; ");
                should_stop = true;
            }

            if should_stop {
                println!("{} stopping watering...", stop_reasons);
                water_port.set_low()?;
            } else {
                println!("Conditions met, starting watering...");
                water_port.set_high()?;

                let start_watering_time = SystemTime::now();
                while SystemTime::now().duration_since(start_watering_time)? < Duration::from_secs(WATERING_TIME) {
                    let sensor_value: i32 = 500;  // Replace with actual sensor reading
                    let moisture_percentage = map(sensor_value, DRY_VALUE, WET_VALUE, 0, 100);
                    println!(
                        "During Watering - Soil Moisture Value: {} -> {}%",
                        sensor_value, moisture_percentage
                    );

                    if !(moisture_percentage < 50) || daily_watering_time >= MAX_DAILY_WATERING_TIME {
                        println!("Moisture exceeds 50% or daily limit reached, stopping watering...");
                        break;
                    }

                    std::thread::sleep(Duration::from_secs(1));
                }

                let watering_duration = SystemTime::now().duration_since(start_watering_time)?;
                daily_watering_time += watering_duration.as_secs();
                water_port.set_low()?;
                println!("Watering complete.");
            }

            std::thread::sleep(Duration::from_secs(10));
        }
    } else {
        println!("WiFi connection failed. Continuing without WiFi.");
    }

    Ok(())
}

fn map(x: i32, in_min: i32, in_max: i32, out_min: i32, out_max: i32) -> i32 {
    (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min
}
