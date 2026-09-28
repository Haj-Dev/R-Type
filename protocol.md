# Pseudo network RFC

## Introduction

The pseudo network RFC is a document that outlines the requirements for a pseudo network, which is a network that is not physically connected to the internet. The pseudo network RFC is designed to provide a framework for the development of pseudo networks and to ensure that they meet certain criteria.

## Format

MagicNumberHeaderBody

### Magic number

The magic number is a specific byte that will be implemented later.

### Header
The header of the pseudo network RFC is a single byte that contains the following information:

- 0x00: The header is a pseudo network RFC.
- 0x01: The header is a pseudo network RFC.
- 0x02: The header is a pseudo network RFC.
- 0x03: The header is a pseudo network RFC.
- 0x04: The header is a pseudo network RFC.
- 0x05: The header is a pseudo network RFC.
- 0x06: The header is a pseudo network RFC.
- ...

### Body
The body of the pseudo network RFC depends on the header value. As such, the table below lists the expected body and body size depending on the header.

| Header | Expected Body | Body Size |
|--------|--------------|------------|
| 0x00   | Pseudo network RFC | 1 byte    |
| 0x01   | Pseudo network RFC | 1 byte    |
| 0x02   | Pseudo network RFC | 1 byte    |
| 0x03   | Pseudo network RFC | 1 byte    |
| 0x04   | Pseudo network RFC | 1 byte    |
| 0x05   | Pseudo network RFC | 1 byte    |
| 0x06   | Pseudo network RFC | 1 byte    |
| ...   | ...          | ...       |
