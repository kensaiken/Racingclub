# Fase 2 — Modelo de conducción (ingeniería inversa del EBOOT)

## Fuentes
- `data/cars/<pack>/<coche>/<coche>_phys.xml` — parámetros por coche.
- `EBOOT.elf` (PPU, PPC64 BE, ABI 32 bits, TOC r2 = `0xc6f210`).
- Las SPU **no** contienen lógica de juego (solo ZLib, audio ATRAC/mezcla, captura y Edge Geometry).

## Localización en el binario
- Nombres de campo XML: `.rodata` `0xb23e88`–`0xb242a8`; array de punteros en `0xbe1724`;
  entradas TOC en `r2+0x31e0 … r2+0x32c0`.
- Registro de parámetros en el *Tweaker* (ajuste en vivo de los desarrolladores):
  `0x33b000–0x33f400`, llamada `bl 0x99c9e0(ctx, name, ..., ptrCampo=r8, count=r9, -1, min/max)`.
- Otros usos (carga/simulación): `0x78e000–0x796000`.

## Estructura por rueda (offsets)
| Campo XML | Offset | Nº |
|---|---|---|
| Wheel_MaxSteerAngle* | +0x130 | 1 |
| Wheel_Friction* | +0x134 | 1 |
| Wheel_RollingDrag | +0x144 | 1 |
| Wheel_BrakeTorque | +0x148 | 1 |
| Wheel_HandBrakeTorque | +0x14c | 1 |
| Wheel_HandBrakeInfluence | +0x150 | 1 |
| Pacejka_Lat | +0x158 | 15 (a0–a14) |
| Pacejka_Long | +0x194 | 13 (b0–b12) |
| Susp_SpringLength / Strength / Damping / DriveableExt | +0x1d0 / +0x1d4 / +0x1d8 / +0x1e0 | 1 |
| Pacejka_Lat_Handbrake | +0x1f4 | 15 |
| Pacejka_Long_Handbrake | +0x230 | 13 |

## Estructura del coche (offsets, base = objeto coche)
| Campo | Offset |
|---|---|
| Gen_AirDrag / DragConst / DriveRatio / CarMass | +0x380 / +0x384 / +0x388 / +0x38c |
| Gen_Restitution / dFriction / sFriction | +0x3b0 / +0x3b4 / +0x3b8 |
| Gen_CenterOfMass (3) / Inertia (3) / Force_Offset (3) | +0x3c0 / +0x3d0 / +0x3e0 |
| Gen_RestoreSteerSpeed / SteerSpeed / PedalSpeed / HandbrakeSpeed | +0x428 / +0x42c / +0x430 / +0x434 |
| Gen_HandbrakeAfterTouch / DefaultAfterTouch | +0x44c / +0x450 |
| Gen_EnableAnims / AirOrientateCar / OverrideCollision | +0x990 / +0x994 / +0x998 |

## Estructura de motor/caja (base propia)
| Campo | Offset |
|---|---|
| Gear_Final_Drive | +0x04 |
| Gear_Num | +0x28 |
| Gear_Clutch_Timer | +0x44 |
| TC_Num_Points / TC_Torque_Mul | +0x74 / +0x78 |

## Neumático: Pacejka '94 (hipótesis validada numéricamente)
Fz en kN, deslizamiento lateral α en grados, longitudinal κ en %.

Lateral:
```
C  = a0
D  = Fz·(a1·Fz + a2)                    // Mini: 2.8·(−22.1·2.8+1000) ≈ 2627 N  → μ≈0.94
BCD= a3·sin(2·atan(Fz/a4))·(1 − a5·|γ|)
B  = BCD/(C·D)
E  = a6·Fz + a7
Sh = a8·γ + a9·Fz + a10
Sv = (a11·Fz·γ + a12·Fz + a13)·Fz        (a14: reservado)
Fy = D·sin(C·atan(B·x − E·(B·x − atan(B·x)))) + Sv,   x = α + Sh
```
Longitudinal:
```
C  = b0
D  = Fz·(b1·Fz + b2)
BCD= (b3·Fz² + b4·Fz)·exp(−b5·Fz)
B  = BCD/(C·D)
E  = b6·Fz² + b7·Fz + b8
Sh = b9·Fz + b10,  Sv = b11·Fz + b12
Fx = D·sin(C·atan(B·x − E·(B·x − atan(B·x)))) + Sv,   x = κ + Sh
```
Pendiente: confirmar en el desensamblado (función que recibe el puntero a +0x158) y el
factor `Wheel_Friction*` (probablemente multiplica D).
