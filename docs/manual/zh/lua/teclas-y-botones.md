# 按键和按钮名称

## 用于 `key` 和 `keyPressed`

| 平台 | 名称 |
|---|---|
| PC、Android 和 Web | `"a"` … `"z"`（小写）、`"0"` … `"9"`、`"up"`、`"down"`、`"left"`、`"right"`、`"space"`、`"enter"`、`"shift"`、`"ctrl"` |
| Nokia N95（手机键盘） | `"up"`、`"down"`、`"left"`、`"right"`、`"enter"`（方向键中间的键也会发送 `"space"`）、`"softleft"`、`"softright"`、`"0"` … `"9"`、`"*"`、`"#"` |

> [!NOTE]
> Esc 没有对应的名称：在编辑器中，Esc 会暂停游戏并释放鼠标。F 键和 Tab 也没有。

## 用于 `button` 和 `buttonPressed`（手柄）

`"a"`、`"b"`、`"x"`、`"y"`、`"up"`、`"down"`、`"left"`、`"right"`（十字键）。摇杆不动时，十字键的方向也会驱动 `stick("left")`。

此外还有：`"l1"`、`"r1"`（肩键），`"l2"`、`"r2"`（当作按钮使用的扳机：按下超过一半即算按下；要读取模拟值请用 `trigger`），`"l3"`、`"r3"`（按下摇杆），`"start"` 和 `"select"`。这些名称来自 PlayStation 手柄；在 Xbox 手柄上分别是 LB / RB、LT / RT 和 Back。字母对应的是按钮的位置，而不是按钮上的图案：`"a"` 是下方的键（叉 ×），`"b"` 是右边的键（圆 ○），`"x"` 是左边的键（方块 □），`"y"` 是上方的键（三角 △）。
