# cdp-v01 演示资产来源（PROVENANCE）

本目录为 GMP-ISE 内置演示「V01 单轴拉伸」的 vendor 输入资产。

## 文件

| 文件 | 来源 | SHA-256 |
|---|---|---|
| `uniaxial_compression_mesh.e` | `/Users/a123/Desktop/code/damASR/docs/examples/cdp-v01-single-tension/input/uniaxial_compression_mesh.e` | `51d296cd1e43d8bb47e2984f49b9ca8aef29db47caf9bb34dbe93974a212f03e` |

- 来源仓库：`damASR`（系统集成/回归基线仓），V01 tension 冻结基线
  `docs/examples/cdp-v01-single-tension/`。
- 拷贝日期：2026-09-27；拷贝后逐字节一致（SHA-256 相同）。
- 语义：单混凝土立方体单轴（z 向）拉伸 Exodus 网格，1 个体组
  `concrete_cube__concrete`、2 个面组 `bottom`/`top`；1331 节点 /
  1000 HEX8 单元。
- 与 `tests/fixtures/cdp-v01/uniaxial_compression_mesh.e` 为同一文件
  （夹具与模板各存一份，互不从属）。

## 冻结声明

- 本目录内容**只读冻结**：不得修改、重生成或替换该 `.e` 文件；
  它是 damASR V01 回归基线的逐字节拷贝，任何变动都会破坏
  「演示案例 › 载入 V01 单轴拉伸」与巡览合同
  `v01_demo_menu_contract` / `cdp_v01_structured_reproduction_contract`
  的可复现性。
- 若 damASR 上游基线更新，须以新 SHA-256 整文件替换并同步更新本文件、
  巡览断言（网格节点/单元计数与 SHA）与手册截图。
