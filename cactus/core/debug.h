#pragma once

#include <cactus/io/writer.h>

#include <string>

namespace cactus {

class FiberImpl;
class Scheduler;

void PrintCactus(IBufferedWriter* writer);

/**
 * DebugPrintCactus печатает в stderr дерево активных fiber-ов.
 *
 * Функцию безопасно звать из обработчика сигналов.
 */
void DebugPrintCactus();

/**
 * SetFiberName устанавливает имя текущего fiber-а. Имя будет отображаться в выводе DebugPrintCactus
 * и в gdb.
 */
void SetFiberName(const char* name);
void SetFiberName(const std::string& name);

}  // namespace cactus
