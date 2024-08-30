// Copyright (C) 2024 Vladislav Nepogodin
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

pub mod scx_loader_config;

use std::fs;
use std::io::Write;

use anyhow::Result;

#[cxx::bridge(namespace = "cachyos_km")]
mod ffi {

    #[derive(Debug, PartialEq, Default, Serialize, Deserialize)]
    #[serde(default)]
    pub struct Config {
        pub hardly_check: bool,
        pub per_gov_check: bool,
        pub tcp_bbr3_check: bool,
        pub auto_optim_check: bool,

        pub cachy_config_check: bool,
        pub nconfig_check: bool,
        pub menuconfig_check: bool,
        pub xconfig_check: bool,
        pub gconfig_check: bool,
        pub localmodcfg_check: bool,
        pub numa_check: bool,
        pub damon_check: bool,
        pub builtin_zfs_check: bool,
        pub builtin_nvidia_check: bool,
        pub builtin_nvidia_open_check: bool,
        pub build_debug_check: bool,

        pub hz_ticks_combo: String,
        pub tickrate_combo: String,
        pub preempt_combo: String,
        pub hugepage_combo: String,
        pub lto_combo: String,

        pub cpu_opt_combo: String,
        pub custom_name_edit: String,
    }

    extern "Rust" {
        fn parse_config_file(filepath: &str) -> Result<Config>;
        fn parse_config(content: &str) -> Result<Config>;
        fn write_config_file(config_ref: &Config, filepath: &str) -> Result<()>;
    }
}

pub fn parse_config_file(filepath: &str) -> Result<ffi::Config> {
    let file_content = fs::read_to_string(filepath)?;
    let config: ffi::Config = toml::from_str(&file_content)?;
    Ok(config)
}

pub fn parse_config(content: &str) -> Result<ffi::Config> {
    let config: ffi::Config = toml::from_str(content)?;
    Ok(config)
}

pub fn write_config_file(config_ref: &ffi::Config, filepath: &str) -> Result<()> {
    let toml_content = toml::to_string(config_ref)?;

    let mut file_obj = fs::File::create(filepath)?;
    file_obj.write_all(toml_content.as_bytes())?;

    Ok(())
}
