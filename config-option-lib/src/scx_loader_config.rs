// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2024 Vladislav Nepogodin <vnepogodin@cachyos.org>

// This software may be used and distributed according to the terms of the
// GNU General Public License version 2.

use std::collections::HashMap;
use std::fs;
use std::io::Write;
use std::path::Path;

use anyhow::{Context, Result};
use serde::{Deserialize, Serialize};

#[cxx::bridge(namespace = "scx_loader")]
mod ffi {
    extern "Rust" {
        type Config;

        /// Initialize config from config path, if the file doesn't exist config with default values
        /// will be created.
        fn init_config_file(config_path: &str) -> Result<Box<Config>>;

        /// Write the config to the file.
        fn write_config_file(&self, filepath: &str) -> Result<()>;

        /// Retrieves default scheduler if set, overwise returns Err
        fn get_default_scheduler(&self) -> Result<String>;

        /// Retrieves default mode if set, overwise returns Auto
        fn get_default_mode(&self) -> u32;

        /// Get the scx flags for the given sched mode
        fn get_scx_flags_for_mode(
            &self,
            supported_sched: &str,
            sched_mode: u32,
        ) -> Result<Vec<String>>;

        /// Set the default scheduler with default mode
        fn set_scx_sched_with_mode(&mut self, supported_sched: &str, sched_mode: u32)
            -> Result<()>;
    }
}

// TODO(vnepogodin): instead of copying code from scx_loader, the scx_loader should be crate with
// provided functions and traits which we call here

#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum SupportedSched {
    #[serde(rename = "scx_bpfland")]
    Bpfland,
    #[serde(rename = "scx_rusty")]
    Rusty,
    #[serde(rename = "scx_lavd")]
    Lavd,
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub enum SchedMode {
    /// Default values for the scheduler
    Auto = 0,
    /// Applies flags for better gaming experience
    Gaming = 1,
    /// Applies flags for lower power usage
    PowerSave = 2,
    /// Starts scheduler in low latency mode
    LowLatency = 3,
}

impl TryFrom<u32> for SchedMode {
    type Error = anyhow::Error;

    fn try_from(value: u32) -> Result<Self> {
        match value {
            0 => Ok(SchedMode::Auto),
            1 => Ok(SchedMode::Gaming),
            2 => Ok(SchedMode::PowerSave),
            3 => Ok(SchedMode::LowLatency),
            _ => anyhow::bail!("SchedMode with such value doesn't exist"),
        }
    }
}

#[derive(Debug, PartialEq, Default, Serialize, Deserialize)]
#[serde(default)]
pub struct Config {
    pub default_sched: Option<SupportedSched>,
    pub default_mode: Option<SchedMode>,
    pub scheds: HashMap<String, Sched>,
}

#[derive(Debug, PartialEq, Serialize, Deserialize)]
pub struct Sched {
    pub auto_mode: Option<Vec<String>>,
    pub gaming_mode: Option<Vec<String>>,
    pub lowlatency_mode: Option<Vec<String>>,
    pub powersave_mode: Option<Vec<String>>,
}

fn init_config_file(config_path: &str) -> Result<Box<Config>> {
    let config = init_config(config_path).context("Failed to initialize config")?;
    Ok(Box::new(config))
}

impl Config {
    fn write_config_file(&self, filepath: &str) -> Result<()> {
        let toml_content = toml::to_string(self)?;

        let mut file_obj = fs::File::create(filepath)?;
        file_obj.write_all(toml_content.as_bytes())?;

        Ok(())
    }

    fn get_default_scheduler(&self) -> Result<String> {
        if let Some(default_sched) = &self.default_sched {
            Ok(get_name_from_scx(default_sched).to_owned())
        } else {
            anyhow::bail!("Default scheduler is not set")
        }
    }

    fn get_default_mode(&self) -> u32 {
        if let Some(sched_mode) = &self.default_mode {
            sched_mode.clone() as u32
        } else {
            SchedMode::Auto as u32
        }
    }

    fn get_scx_flags_for_mode(
        &self,
        supported_sched: &str,
        sched_mode: u32,
    ) -> Result<Vec<String>> {
        let scx_sched = get_scx_from_str(supported_sched)?;
        let sched_mode: SchedMode = sched_mode.try_into()?;
        let args = get_scx_flags_for_mode(self, &scx_sched, sched_mode);
        Ok(args)
    }

    fn set_scx_sched_with_mode(&mut self, supported_sched: &str, sched_mode: u32) -> Result<()> {
        let scx_sched = get_scx_from_str(supported_sched)?;
        let sched_mode: SchedMode = sched_mode.try_into()?;

        self.default_sched = Some(scx_sched);
        self.default_mode = Some(sched_mode);

        Ok(())
    }
}

/// Initialize config from first found config path, overwise fallback to default config
pub fn init_config(config_path: &str) -> Result<Config> {
    if Path::new(config_path).exists() {
        parse_config_file(config_path)
    } else {
        Ok(get_default_config())
    }
}

pub fn parse_config_file(filepath: &str) -> Result<Config> {
    let file_content = fs::read_to_string(filepath)?;
    parse_config_content(&file_content)
}

fn parse_config_content(file_content: &str) -> Result<Config> {
    if file_content.is_empty() {
        anyhow::bail!("The config file is empty!")
    }
    let config: Config = toml::from_str(file_content)?;
    Ok(config)
}

pub fn get_default_config() -> Config {
    Config {
        default_sched: None,
        default_mode: Some(SchedMode::Auto),
        scheds: HashMap::from([
            ("scx_bpfland".to_string(), get_default_sched_for_config(&SupportedSched::Bpfland)),
            ("scx_rusty".to_string(), get_default_sched_for_config(&SupportedSched::Rusty)),
            ("scx_lavd".to_string(), get_default_sched_for_config(&SupportedSched::Lavd)),
        ]),
    }
}

/// Get the scx flags for the given sched mode
pub fn get_scx_flags_for_mode(
    config: &Config,
    scx_sched: &SupportedSched,
    sched_mode: SchedMode,
) -> Vec<String> {
    if let Some(sched_config) = config.scheds.get(get_name_from_scx(scx_sched)) {
        let scx_flags = extract_scx_flags_from_config(sched_config, &sched_mode);

        // try to exact flags from config, otherwise fallback to hardcoded default
        scx_flags.unwrap_or({
            get_default_scx_flags_for_mode(scx_sched, sched_mode)
                .into_iter()
                .map(String::from)
                .collect()
        })
    } else {
        get_default_scx_flags_for_mode(scx_sched, sched_mode)
            .into_iter()
            .map(String::from)
            .collect()
    }
}

/// Extract the scx flags from config
fn extract_scx_flags_from_config(
    sched_config: &Sched,
    sched_mode: &SchedMode,
) -> Option<Vec<String>> {
    match sched_mode {
        SchedMode::Gaming => sched_config.gaming_mode.clone(),
        SchedMode::LowLatency => sched_config.lowlatency_mode.clone(),
        SchedMode::PowerSave => sched_config.powersave_mode.clone(),
        SchedMode::Auto => sched_config.auto_mode.clone(),
    }
}

/// Get Sched object for configuration object
fn get_default_sched_for_config(scx_sched: &SupportedSched) -> Sched {
    Sched {
        auto_mode: Some(
            get_default_scx_flags_for_mode(scx_sched, SchedMode::Auto)
                .into_iter()
                .map(String::from)
                .collect(),
        ),
        gaming_mode: Some(
            get_default_scx_flags_for_mode(scx_sched, SchedMode::Gaming)
                .into_iter()
                .map(String::from)
                .collect(),
        ),
        lowlatency_mode: Some(
            get_default_scx_flags_for_mode(scx_sched, SchedMode::LowLatency)
                .into_iter()
                .map(String::from)
                .collect(),
        ),
        powersave_mode: Some(
            get_default_scx_flags_for_mode(scx_sched, SchedMode::PowerSave)
                .into_iter()
                .map(String::from)
                .collect(),
        ),
    }
}

/// Get the default scx flags for the given sched mode
fn get_default_scx_flags_for_mode(scx_sched: &SupportedSched, sched_mode: SchedMode) -> Vec<&str> {
    match scx_sched {
        SupportedSched::Bpfland => match sched_mode {
            SchedMode::Gaming => vec!["-m", "performance"],
            SchedMode::LowLatency => vec!["-k", "-s", "5000", "-l", "5000"],
            SchedMode::PowerSave => vec!["-m", "powersave"],
            SchedMode::Auto => vec![],
        },
        SupportedSched::Lavd => match sched_mode {
            SchedMode::Gaming | SchedMode::LowLatency => vec!["--performance"],
            SchedMode::PowerSave => vec!["--powersave"],
            // NOTE: potentially adding --auto in future
            SchedMode::Auto => vec![],
        },
        // scx_rusty doesn't support any of these modes
        SupportedSched::Rusty => vec![],
    }
}

/// Get the scx trait from the given scx name or return error if the given scx name is not supported
fn get_scx_from_str(scx_name: &str) -> Result<SupportedSched> {
    match scx_name {
        "scx_bpfland" => Ok(SupportedSched::Bpfland),
        "scx_rusty" => Ok(SupportedSched::Rusty),
        "scx_lavd" => Ok(SupportedSched::Lavd),
        _ => Err(anyhow::anyhow!("{scx_name} is not supported")),
    }
}

/// Get the scx name from the given scx trait
fn get_name_from_scx(supported_sched: &SupportedSched) -> &'static str {
    match supported_sched {
        SupportedSched::Bpfland => "scx_bpfland",
        SupportedSched::Rusty => "scx_rusty",
        SupportedSched::Lavd => "scx_lavd",
    }
}

#[cfg(test)]
mod tests {
    use crate::scx_loader_config::*;

    #[test]
    fn test_default_config() {
        let config_str = r#"
default_mode = "Auto"

[scheds.scx_bpfland]
auto_mode = []
gaming_mode = ["-m", "performance"]
lowlatency_mode = ["-k", "-s", "5000", "-l", "5000"]
powersave_mode = ["-m", "powersave"]

[scheds.scx_rusty]
auto_mode = []
gaming_mode = []
lowlatency_mode = []
powersave_mode = []

[scheds.scx_lavd]
auto_mode = []
gaming_mode = ["--performance"]
lowlatency_mode = ["--performance"]
powersave_mode = ["--powersave"]
"#;

        let parsed_config = parse_config_content(config_str).expect("Failed to parse config");
        let expected_config = get_default_config();

        assert_eq!(parsed_config, expected_config);
    }

    #[test]
    fn test_simple_fallback_config_flags() {
        let config_str = r#"
default_mode = "Auto"
"#;

        let parsed_config = parse_config_content(config_str).expect("Failed to parse config");

        let bpfland_flags =
            get_scx_flags_for_mode(&parsed_config, &SupportedSched::Bpfland, SchedMode::Gaming);
        let expected_flags =
            get_default_scx_flags_for_mode(&SupportedSched::Bpfland, SchedMode::Gaming);
        assert_eq!(bpfland_flags.iter().map(|x| x.as_str()).collect::<Vec<&str>>(), expected_flags);
    }

    #[test]
    fn test_sched_fallback_config_flags() {
        let config_str = r#"
default_mode = "Auto"

[scheds.scx_lavd]
auto_mode = ["--help"]
"#;

        let parsed_config = parse_config_content(config_str).expect("Failed to parse config");

        let lavd_flags =
            get_scx_flags_for_mode(&parsed_config, &SupportedSched::Lavd, SchedMode::Gaming);
        let expected_flags =
            get_default_scx_flags_for_mode(&SupportedSched::Lavd, SchedMode::Gaming);
        assert_eq!(lavd_flags.iter().map(|x| x.as_str()).collect::<Vec<&str>>(), expected_flags);

        let lavd_flags =
            get_scx_flags_for_mode(&parsed_config, &SupportedSched::Lavd, SchedMode::Auto);
        assert_eq!(lavd_flags.iter().map(|x| x.as_str()).collect::<Vec<&str>>(), vec!["--help"]);
    }

    #[test]
    fn test_empty_config() {
        let config_str = "";
        let result = parse_config_content(config_str);
        assert!(result.is_err());
    }
}
