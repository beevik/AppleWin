#pragma once

Update_t _CmdDisasmDataDefByteX(int nArgs);
Update_t _CmdDisasmDataDefWordX(int nArgs);

// Data Disassembler ______________________________________________________________________________

DisasmData_t * Disassembly_IsDataAddress(WORD nAddress);

void Disassembly_AddData(DisasmData_t tData);
void Disassembly_GetData(WORD nBaseAddress, const DisasmData_t * pData, DisasmLine_t & line);
void Disassembly_DelData(DisasmData_t tData);
DisasmData_t * Disassembly_Enumerate(DisasmData_t * pCurrent = NULL);

extern std::vector<DisasmData_t> g_aDisassemblerData;
