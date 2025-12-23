# 🐸 Shijima-Qt API 文档（中文版）

**基础地址：** `http://127.0.0.1:32456/guyi/api/v1`

---

## 🧩 GET `/mascots`

返回当前屏幕上正在显示的桌宠列表。

**示例响应：**

```json
{
    "mascots": [
        {
            "active_behavior": "ClimbIEWall",
            "anchor": {
                "x": 67.2,
                "y": 225.63864462595598
            },
            "data_id": 0,
            "id": 35,
            "name": "Default Mascot"
        },
        {
            "active_behavior": "Fall",
            "anchor": {
                "x": 368,
                "y": 863
            },
            "data_id": 0,
            "id": 36,
            "name": "Default Mascot"
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
    "name": "string",
    "data_id": "int",
    "anchor": {
        "x": "double",
        "y": "double"
    },
    "behavior": "string"
}
```

**示例请求：**

```json
{
    "name": "Default Mascot",
    "anchor": {
        "x": 150,
        "y": 150
    },
    "behavior": "SplitIntoTwo"
}
```

**示例响应：**

```json
{
    "mascot": {
        "active_behavior": null,
        "anchor": {
            "x": 150,
            "y": 150
        },
        "data_id": 0,
        "id": 40,
        "name": "Default Mascot"
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
    "mascot": {
        "active_behavior": "ClimbIEWall",
        "anchor": {
            "x": 67.2,
            "y": 225.63864462595598
        },
        "data_id": 0,
        "id": 35,
        "name": "Default Mascot"
    }
}
```

---

## ✏️ PUT `/mascots/:id`

修改指定桌宠的状态。

**请求格式：**

```json
{
    "id": "int",
    "anchor": {
        "x": "double",
        "y": "double"
    },
    "behavior": "string"
}
```

**示例请求：**

```json
{
    "id": 4,
    "anchor": {
        "x": 150,
        "y": 150
    },
    "behavior": "SplitIntoTwo"
}
```

**示例响应：**

```json
{
    "mascot": {
        "active_behavior": "SitDown",
        "anchor": {
            "x": 150,
            "y": 150
        },
        "data_id": 79,
        "id": 4,
        "name": "Jenny"
    }
}
```

---

## 📦 GET `/loadedMascots`

返回当前已加载（可用）的桌宠列表。

**示例响应：**

```json
{
    "loaded_mascots": [
        {
            "id": 0,
            "name": "Default Mascot"
        },
        {
            "id": 79,
            "name": "Jenny"
        },
        {
            "id": 78,
            "name": "niko"
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
    "loaded_mascot": {
        "id": 79,
        "name": "Jenny"
    }
}
```

---

## 🖼️ GET `/loadedMascots/:id/preview.png`

返回指定桌宠的预览图片。

---

## 💬 POST `/mascots/:id/message`

在指定桌宠上方显示一条对话气泡。

**请求格式：**

```json
{
    "text": "string",
    "duration": "int (可选，单位：毫秒)"
}
```

**示例请求：**

```json
{
    "text": "Hello! I'm a shimeji!",
    "duration": 5000
}
```

**示例响应：**

```json
{
    "success": true
}
```

**说明：**
- 如果 `duration` 为 `0` 或未填写，消息会一直显示，直到手动关闭。
- 如果 `duration` > 0，则消息会在指定毫秒数后自动消失。

---

## 🚫 DELETE `/mascots/:id/message`

隐藏指定桌宠的气泡消息。

**示例响应：**

```json
{
    "success": true
}
```
