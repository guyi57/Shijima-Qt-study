# 🐸 Shijima-Qt API 文档（中文版）

**基础地址：** `http://127.0.0.1:32456/guyi/api/v1`

---

## 🧩 GET `/mascots`

返回当前屏幕上正在显示的桌宠列表。

**示例响应：**

```json
{
    \"mascots\": [
        {
            \"active_behavior\": \"ClimbIEWall\",
            \"anchor\": {
                \"x\": 67.2,
                \"y\": 225.63864462595598
            },
            \"data_id\": 0,
            \"id\": 35,
            \"name\": \"Default Mascot\"
        },
        {
            \"active_behavior\": \"Fall\",
            \"anchor\": {
                \"x\": 368,
                \"y\": 863
            },
            \"data_id\": 0,
            \"id\": 36,
            \"name\": \"Default Mascot\"
        }
    ]
}
```

---

## 🐣 POST `/mascots`

生成（召唤）一个新的桌宠。  
参数中必须指定 `name` 或 `data_id` 之一。

**请求格式：**

```json
{
    \"name\": \"string\",
    \"data_id\": \"int\",
    \"anchor\": {
        \"x\": \"double\",
        \"y\": \"double\"
    },
    \"behavior\": \"string\"
}
```

**示例请求：**

```json
{
    \"name\": \"Default Mascot\",
    \"anchor\": {
        \"x\": 150,
        \"y\": 150
    },
    \"behavior\": \"SplitIntoTwo\"
}
```

**示例响应：**

```json
{
    \"mascot\": {
        \"active_behavior\": null,
        \"anchor\": {
            \"x\": 150,
            \"y\": 150
        },
        \"data_id\": 0,
        \"id\": 40,
        \"name\": \"Default Mascot\"
    }
}
```

---

## 🧹 DELETE `/mascots`

关闭（移除）所有正在屏幕上的桌宠。

---

## 🔍 GET `/mascots/:id`

获取指定桌宠的详细信息。

**示例响应：**

```json
{
    \"mascot\": {
        \"active_behavior\": \"ClimbIEWall\",
        \"anchor\": {
            \"x\": 67.2,
            \"y\": 225.63864462595598
        },
        \"data_id\": 0,
        \"id\": 35,
        \"name\": \"Default Mascot\"
    }
}
```

---

## ✏️ PUT `/mascots/:id`

修改指定桌宠的状态。

**请求格式：**

```json
{
    \"id\": \"int\",
    \"anchor\": {
        \"x\": \"double\",
        \"y\": \"double\"
    },
    \"behavior\": \"string\"
}
```

**示例请求：**

```json
{
    \"id\": 4,
    \"anchor\": {
        \"x\": 150,
        \"y\": 150
    },
    \"behavior\": \"SplitIntoTwo\"
}
```

**示例响应：**

```json
{
    \"mascot\": {
        \"active_behavior\": \"SitDown\",
        \"anchor\": {
            \"x\": 150,
            \"y\": 150
        },
        \"data_id\": 79,
        \"id\": 4,
        \"name\": \"Jenny\"
    }
}
```

---

## 📦 GET `/loadedMascots`

返回当前已加载（可用）的桌宠列表。

**示例响应：**

```json
{
    \"loaded_mascots\": [
        {
            \"id\": 0,
            \"name\": \"Default Mascot\"
        },
        {
            \"id\": 79,
            \"name\": \"Jenny\"
        },
        {
            \"id\": 78,
            \"name\": \"niko\"
        }
    ]
}
```

---

## 📄 GET `/loadedMascots/:id`

返回指定已加载桌宠的信息。

**示例响应：**

```json
{
    \"loaded_mascot\": {
        \"id\": 79,
        \"name\": \"Jenny\"
    }
}
```

---

## 🖼️ GET `/loadedMascots/:id/preview.png`

返回指定桌宠的预览图片。

---

## 💬 POST `/mascots/:id/message`

在指定桌宠上方显示一条对话气泡，支持快捷打开应用或 URL 动作。

**请求参数：**

| 参数名 | 类型 | 必填 | 说明 |
| :--- | :--- | :--- | :--- |
| `text` | string | 是 | 气泡要显示的正文内容 |
| `duration` | int | 否 | 显示时长（毫秒），默认 `0`（不自动关闭） |
| `app` / `app_name` | string | 否 | 应用名称（如 `"Safari"`, `"WeChat"`, `"VSCode"`） |
| `bundle_id` | string | 否 | 应用包名/Bundle ID（如 `"com.apple.Safari"`, `"com.tencent.xinWeChat"`） |
| `url` | string | 否 | 网页链接或 Deep Link（如 `"https://..."`, `"vscode://..."`） |

**示例 1（通过应用名打开）：**

```json
{
    \"text\": \"您有一条待办事项，点击打开微信处理\",
    \"duration\": 10000,
    \"app\": \"WeChat\"
}
```

**示例 2（通过 Bundle ID 包名打开）：**

```json
{
    \"text\": \"代码构建完成，已定位到项目目录\",
    \"duration\": 12000,
    \"bundle_id\": \"com.microsoft.VSCode\"
}
```

**示例 3（打开网页或协议链接）：**

```json
{
    \"text\": \"点击下方按钮查看 GitHub 项目主页：\",
    \"duration\": 15000,
    \"url\": \"https://github.com/guyi57/Shijima-Qt-study\"
}
```

**示例响应：**

```json
{
    \"success\": true
}
```

**效果：**
气泡弹窗顶部会自动渲染出绿色的 **【🚀 打开应用】** 按钮，点击即可直接前台拉起对应应用或打开网页！

---

## 🚫 DELETE `/mascots/:id/message`

隐藏指定桌宠的气泡消息。

**示例响应：**

```json
{
    \"success\": true
}
```
