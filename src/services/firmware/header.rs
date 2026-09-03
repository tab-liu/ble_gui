//! 固件头识别：IOT 整包（>1MB，不解析 POWEROAK）或自动区分 8 位 / TI 16 位 Word。
//!
//! 类型枚举对齐 `ref/dev`：`DEVICE_IOT = 0`，不要用 Studio 显示表里的「IOT=4」。

/// 大于此大小视为 IOT 整包，跳过 POWEROAK 头。
pub const IOT_MIN_BYTES: u64 = 1024 * 1024;
/// 拒绝过大文件，避免误把非固件拖进来。
pub const FIRMWARE_MAX_BYTES: u64 = 16 * 1024 * 1024;

const MAGIC: &[u8; 8] = b"POWEROAK";
const ESP_APP_DESC_MAGIC: u32 = 0xABCD_5432;

const TYPE_NAMES: &[&str] = &[
    "IOT",
    "ARM",
    "DSP",
    "BMS",
    "PACK BA",
    "PACK BCU",
    "PACK BMU",
    "PACK BMS",
    "PACK M1",
    "PACK Safety",
    "PACK HV",
    "HMI1",
    "HMI2",
    "RF",
    "DC HUB",
    "AC HUB",
    "DC-DC",
    "AT1 ARM",
    "Panel ARM",
    "Synlink",
];

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HeaderLayout {
    /// ESP/IOT 整包，无 POWEROAK 头。
    IotRaw,
    /// 普通 MCU：每字节紧密排列。
    Packed8,
    /// TI C2000：逻辑字节占一个 16-bit word，取低字节。
    TiWord16,
}

impl HeaderLayout {
    pub fn label(self) -> &'static str {
        match self {
            Self::IotRaw => "IOT 整包",
            Self::Packed8 => "8 位布局",
            Self::TiWord16 => "TI 16 位 Word 布局",
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct FirmwareInfo {
    pub layout: HeaderLayout,
    pub type_code: u8,
    pub version: u32,
    pub image_size: u32,
    pub crc32: u32,
    pub dev_model: String,
    /// IOT 整包时可能从 ESP app_desc 读到的版本字符串。
    pub esp_version: String,
    /// 版本字段在文件中的来源（偏移 / 结构体字段），供 UI 与日志对照。
    pub parse_source: String,
}

impl FirmwareInfo {
    pub fn type_name(&self) -> String {
        type_name(self.type_code)
    }

    pub fn version_text(&self) -> String {
        // ESP/IOT 整包的 app_desc.version 是 git describe，不是业务版本号，界面不展示。
        if self.layout == HeaderLayout::IotRaw {
            return "None".into();
        }
        if self.version == 0 {
            return "—".into();
        }
        format!("{} / 0x{:08X}", self.version, self.version)
    }
}

pub fn type_name(code: u8) -> String {
    TYPE_NAMES
        .get(code as usize)
        .map(|s| (*s).to_string())
        .unwrap_or_else(|| format!("未知({code})"))
}

/// 按文件大小 + 开头字节分类。`data` 至少应覆盖头区（IOT 建议 ≥256 字节）。
pub fn classify(file_size: u64, data: &[u8]) -> Result<FirmwareInfo, String> {
    if file_size == 0 {
        return Err("文件为空".into());
    }
    if file_size > FIRMWARE_MAX_BYTES {
        return Err(format!(
            "文件过大（{} 字节，上限 {} MB）",
            file_size,
            FIRMWARE_MAX_BYTES / (1024 * 1024)
        ));
    }
    if file_size > IOT_MIN_BYTES {
        log::debug!(
            target: "ble_gui::firmware",
            "按大小判定为 IOT 整包：{file_size} 字节 > {IOT_MIN_BYTES}，跳过 POWEROAK 头"
        );
        if let Some(pos) = find_bytes(data, MAGIC) {
            log::debug!(
                target: "ble_gui::firmware",
                "文件中仍发现 POWEROAK（未使用）@ 0x{pos:X}（{pos}）"
            );
        }
        let extracted = extract_esp_app_desc(data);
        return Ok(FirmwareInfo {
            layout: HeaderLayout::IotRaw,
            type_code: 0,
            version: 0,
            image_size: 0,
            crc32: 0,
            dev_model: extracted.project_name,
            esp_version: extracted.version,
            parse_source: extracted.source,
        });
    }

    if let Some(info) = parse_packed8(data) {
        return Ok(info);
    }
    if let Some(info) = parse_ti16(data) {
        return Ok(info);
    }
    Err("无法识别固件头：不是 POWEROAK（8 位或 TI 16 位），且小于 1MB 不能当作 IOT 整包".into())
}

fn parse_packed8(data: &[u8]) -> Option<FirmwareInfo> {
    if data.len() < 53 {
        return None;
    }
    if &data[0..8] != MAGIC {
        return None;
    }
    let image_size = u32::from_le_bytes(data[25..29].try_into().ok()?);
    if image_size == 0 {
        return None;
    }
    Some(FirmwareInfo {
        layout: HeaderLayout::Packed8,
        type_code: data[8],
        version: u32::from_le_bytes(data[21..25].try_into().ok()?),
        image_size,
        crc32: u32::from_le_bytes(data[29..33].try_into().ok()?),
        dev_model: ascii_trim(&data[9..21]),
        esp_version: String::new(),
        parse_source: "POWEROAK 8 位头 · Version u32 @ 偏移 21 (0x15)".into(),
    })
}

fn parse_ti16(data: &[u8]) -> Option<FirmwareInfo> {
    if data.len() < 0x38 {
        return None;
    }
    let mut magic = [0u8; 8];
    for i in 0..8 {
        magic[i] = u16::from_le_bytes(data[i * 2..i * 2 + 2].try_into().ok()?) as u8;
    }
    if &magic != MAGIC {
        return None;
    }
    let type_code = u16::from_le_bytes(data[0x10..0x12].try_into().ok()?) as u8;
    let mut model_raw = [0u8; 12];
    for i in 0..12 {
        let off = 0x12 + i * 2;
        model_raw[i] = u16::from_le_bytes(data[off..off + 2].try_into().ok()?) as u8;
    }
    let version = u16le(data, 0x2C)? as u32 | ((u16le(data, 0x2E)? as u32) << 16);
    let image_size = u16le(data, 0x30)? as u32 | ((u16le(data, 0x32)? as u32) << 16);
    let crc32 = u16le(data, 0x34)? as u32 | ((u16le(data, 0x36)? as u32) << 16);
    if image_size == 0 {
        return None;
    }
    Some(FirmwareInfo {
        layout: HeaderLayout::TiWord16,
        type_code,
        version,
        image_size,
        crc32,
        dev_model: ascii_trim(&model_raw),
        esp_version: String::new(),
        parse_source: "POWEROAK TI 16 位头 · Version @ 0x2C".into(),
    })
}

fn u16le(data: &[u8], offset: usize) -> Option<u16> {
    Some(u16::from_le_bytes(data.get(offset..offset + 2)?.try_into().ok()?))
}

fn ascii_trim(bytes: &[u8]) -> String {
    let end = bytes
        .iter()
        .position(|&b| b == 0 || b == 0xFF)
        .unwrap_or(bytes.len());
    String::from_utf8_lossy(&bytes[..end])
        .trim()
        .to_string()
}

/// ESP-IDF：`esp_image_header_t`(24) + 首段 `esp_image_segment_header_t`(8) 之后是 `esp_app_desc_t`。
const ESP_APP_DESC_STD_OFF: usize = 32;
const APP_DESC_VERSION_OFF: usize = 16;
const APP_DESC_VERSION_LEN: usize = 32;
const APP_DESC_PROJECT_OFF: usize = 48;
const APP_DESC_PROJECT_LEN: usize = 32;
const APP_DESC_IDF_OFF: usize = 112;
const APP_DESC_IDF_LEN: usize = 32;

struct EspAppDesc {
    version: String,
    project_name: String,
    source: String,
}

fn extract_esp_app_desc(data: &[u8]) -> EspAppDesc {
    let hits = find_u32_le(data, ESP_APP_DESC_MAGIC);
    if hits.is_empty() {
        log::debug!(
            target: "ble_gui::firmware",
            "未找到 ESP app_desc magic 0x{ESP_APP_DESC_MAGIC:08X}，IOT 版本留空（仅按文件大小分类）"
        );
        return EspAppDesc {
            version: String::new(),
            project_name: String::new(),
            source: format!(
                "仅按大小 > {IOT_MIN_BYTES} 判为 IOT，文件中无 ESP app_desc (0xABCD5432)"
            ),
        };
    }
    let listed = hits
        .iter()
        .take(8)
        .map(|o| format!("0x{o:X}"))
        .collect::<Vec<_>>()
        .join(", ");
    log::debug!(
        target: "ble_gui::firmware",
        "ESP app_desc magic 0x{ESP_APP_DESC_MAGIC:08X} 共 {} 处：{listed}{}",
        hits.len(),
        if hits.len() > 8 { " …" } else { "" }
    );

    let desc_off = if hits.contains(&ESP_APP_DESC_STD_OFF) {
        ESP_APP_DESC_STD_OFF
    } else {
        hits[0]
    };
    if desc_off != ESP_APP_DESC_STD_OFF {
        log::debug!(
            target: "ble_gui::firmware",
            "标准 app.bin 位置 0x{ESP_APP_DESC_STD_OFF:X} 无 magic，改用第一处 0x{desc_off:X}（可能是带 bootloader 的整片镜像）"
        );
    }

    let version_off = desc_off + APP_DESC_VERSION_OFF;
    let project_off = desc_off + APP_DESC_PROJECT_OFF;
    let idf_off = desc_off + APP_DESC_IDF_OFF;
    let version_raw = slice_len(data, version_off, APP_DESC_VERSION_LEN);
    let project_raw = slice_len(data, project_off, APP_DESC_PROJECT_LEN);
    let idf_raw = slice_len(data, idf_off, APP_DESC_IDF_LEN);
    let version = ascii_trim(version_raw);
    let project_name = ascii_trim(project_raw);
    let idf_ver = ascii_trim(idf_raw);

    log::debug!(
        target: "ble_gui::firmware",
        "使用 esp_app_desc_t @ 文件偏移 0x{desc_off:X}（{desc_off}）"
    );
    log::debug!(
        target: "ble_gui::firmware",
        "  version[32]      @ 0x{version_off:X} = {:?}  hex[{}]",
        version,
        hex_bytes(version_raw)
    );
    log::debug!(
        target: "ble_gui::firmware",
        "  project_name[32] @ 0x{project_off:X} = {:?}  hex[{}]",
        project_name,
        hex_bytes(project_raw)
    );
    log::debug!(
        target: "ble_gui::firmware",
        "  idf_ver[32]      @ 0x{idf_off:X} = {:?}",
        idf_ver
    );

    EspAppDesc {
        source: format!(
            "ESP app_desc.version[32] @ 0x{version_off:X}（desc @ 0x{desc_off:X}，magic=0xABCD5432）"
        ),
        version,
        project_name,
    }
}

fn slice_len(data: &[u8], offset: usize, len: usize) -> &[u8] {
    let end = (offset + len).min(data.len());
    if offset >= data.len() {
        &[]
    } else {
        &data[offset..end]
    }
}

fn hex_bytes(bytes: &[u8]) -> String {
    bytes
        .iter()
        .map(|b| format!("{b:02X}"))
        .collect::<Vec<_>>()
        .join(" ")
}

fn find_u32_le(data: &[u8], value: u32) -> Vec<usize> {
    let needle = value.to_le_bytes();
    find_all(data, &needle)
}

fn find_bytes(data: &[u8], needle: &[u8]) -> Option<usize> {
    data.windows(needle.len()).position(|w| w == needle)
}

fn find_all(data: &[u8], needle: &[u8]) -> Vec<usize> {
    if needle.is_empty() || data.len() < needle.len() {
        return Vec::new();
    }
    let mut hits = Vec::new();
    let mut start = 0;
    while start + needle.len() <= data.len() {
        if let Some(rel) = data[start..].windows(needle.len()).position(|w| w == needle) {
            let abs = start + rel;
            hits.push(abs);
            start = abs + 1;
        } else {
            break;
        }
    }
    hits
}

#[cfg(test)]
mod tests {
    use super::*;

    fn packed8_sample() -> Vec<u8> {
        let mut data = vec![0u8; 64];
        data[0..8].copy_from_slice(MAGIC);
        data[8] = 1; // ARM
        let mut model = [b' '; 12];
        model[..5].copy_from_slice(b"AC180");
        data[9..21].copy_from_slice(&model);
        data[21..25].copy_from_slice(&100650103u32.to_le_bytes());
        data[25..29].copy_from_slice(&4096u32.to_le_bytes());
        data[29..33].copy_from_slice(&0xAABBCCDDu32.to_le_bytes());
        data
    }

    fn ti16_sample() -> Vec<u8> {
        let mut data = vec![0u8; 96];
        for (i, b) in MAGIC.iter().enumerate() {
            data[i * 2] = *b;
            data[i * 2 + 1] = 0;
        }
        data[0x10] = 2; // DSP
        data[0x11] = 0;
        let model = b"INV-DSP";
        for (i, b) in model.iter().enumerate() {
            data[0x12 + i * 2] = *b;
        }
        let ver = 802616u32;
        data[0x2C..0x2E].copy_from_slice(&(ver as u16).to_le_bytes());
        data[0x2E..0x30].copy_from_slice(&((ver >> 16) as u16).to_le_bytes());
        data[0x30..0x32].copy_from_slice(&2048u16.to_le_bytes());
        data[0x32..0x34].copy_from_slice(&0u16.to_le_bytes());
        data[0x34..0x36].copy_from_slice(&0x7788u16.to_le_bytes());
        data[0x36..0x38].copy_from_slice(&0x5566u16.to_le_bytes());
        data
    }

    #[test]
    fn packed8_magic_parsed() {
        let info = classify(64, &packed8_sample()).unwrap();
        assert_eq!(info.layout, HeaderLayout::Packed8);
        assert_eq!(info.type_code, 1);
        assert_eq!(info.type_name(), "ARM");
        assert_eq!(info.version, 100650103);
        assert!(info.dev_model.starts_with("AC180"));
    }

    #[test]
    fn ti16_used_when_packed8_fails() {
        let data = ti16_sample();
        assert!(parse_packed8(&data).is_none());
        let info = classify(96, &data).unwrap();
        assert_eq!(info.layout, HeaderLayout::TiWord16);
        assert_eq!(info.type_code, 2);
        assert_eq!(info.version, 802616);
        assert_eq!(info.image_size, 2048);
    }

    #[test]
    fn oversized_file_is_iot_without_header() {
        let mut data = vec![0u8; 256];
        data[0..8].copy_from_slice(MAGIC); // 即使有 POWEROAK 也不解析
        let info = classify(IOT_MIN_BYTES + 1, &data).unwrap();
        assert_eq!(info.layout, HeaderLayout::IotRaw);
        assert_eq!(info.type_code, 0);
        assert_eq!(info.type_name(), "IOT");
    }

    #[test]
    fn small_garbage_rejected() {
        let data = vec![0u8; 64];
        let err = classify(64, &data).unwrap_err();
        assert!(err.contains("无法识别"));
    }

    #[test]
    fn packed8_and_ti16_magic_do_not_collide() {
        assert!(parse_ti16(&packed8_sample()).is_none());
        assert!(parse_packed8(&ti16_sample()).is_none());
    }

    #[test]
    fn iot_reads_esp_app_desc_version_at_offset_32() {
        let mut data = vec![0u8; 256];
        data[32..36].copy_from_slice(&ESP_APP_DESC_MAGIC.to_le_bytes());
        let ver = b"ap200-v100600106-ap300-8026-tes";
        data[48..48 + ver.len()].copy_from_slice(ver);
        let proj = b"ap200-iot";
        data[80..80 + proj.len()].copy_from_slice(proj);
        let info = classify(IOT_MIN_BYTES + 1, &data).unwrap();
        assert_eq!(info.esp_version, "ap200-v100600106-ap300-8026-tes");
        assert_eq!(info.version_text(), "None");
        assert_eq!(info.dev_model, "ap200-iot");
        assert!(info.parse_source.contains("0x30"));
        assert!(info.parse_source.contains("app_desc.version"));
    }
}
