# log库

## 功能描述
log库是设备接入SDK提供的日志桩，通过使用该库，可以在驱动部署到边缘网关时使用边缘网关真实日志库，实现驱动日志输出文件保存。

## 接口描述
- **[log_init](#log_init)**
- **[log_set_level](#log_set_level)**
- **[log_print](#log_print)**
- **[log_destroy](#log_destroy)**

---
<a name="log_init"></a>
``` c
int  log_init(const char *name, LOG_STORE_TYPE type, LOG_LEVEL lvl, LOG_MODE mode);

```

---
<a name="log_set_level"></a>
``` c
void log_set_level(LOG_LEVEL level);

```

---
<a name="log_print"></a>
``` c
void log_print(unsigned char lvl, const char *color, const char *tag, const char *file, const char *func, unsigned int line, const char *format, ...);

```

---
<a name="log_destroy"></a>
``` c
void log_destroy();

```
