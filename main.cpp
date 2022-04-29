#include <Arduino.h>
#include <stdio.h>
#include <CommandParser.h>

TaskHandle_t xDot_Task;

namespace task_handler
{
  const int MAX_USER_TASKS = 3;

  struct task_data
  {
    TaskHandle_t handle;
    bool handle_in_use = false;
  };

  enum task_error
  {
    noError = 0,
    out_of_user_task_space,
    out_of_mem,
    nothing_to_kill
  };

  task_data tasks[MAX_USER_TASKS];

  int get_free_task_slot()
  {
    // This function will scan through the task array, and return the first free space for the task to spawn in
    int slot = -1;
    for (size_t i = 0; i <= MAX_USER_TASKS - 1; i++)
    {
      if (tasks[i].handle_in_use == false)
      { // If a handle is not in use
        slot = i;
        break;
      }
    }
    return slot;
  }

  task_error spawn_new(TaskFunction_t pvTaskCode, // Task code
                       int prioraty,              // Lower the number, the lower the priorty
                       int stack_depth,           // Ram requirements  (in bytes(i think... it could be words idk idf changed it...))
                       const char *app_name)      // A pretty name for the user
  {
    task_error err = noError;

    int task_slot = get_free_task_slot();
    Serial.println(task_slot);

    if (task_slot == -1)
    {
      err = out_of_user_task_space;
    }

    if (err == noError)
    {
      xTaskCreate(pvTaskCode,
                  app_name,
                  stack_depth,
                  NULL,
                  prioraty,
                  &tasks[task_slot].handle); // task handle for where the task should go in the tasks array...
    }

    if (err == noError)
    {
      tasks[task_slot].handle_in_use = true;
      Serial.println("task spawned with no errors");
    }

    return err;
  }

  task_error kill_task(unsigned int task_number)
  {
    task_error err = noError;

    if ((tasks[task_number].handle_in_use == false) || task_number >= MAX_USER_TASKS)
    {
      err = nothing_to_kill;
    }
    else
    {
      vTaskDelete(tasks[task_number].handle);

      tasks[task_number].handle_in_use = false;
    }

    return err;
  }

  const char *state_to_name(eTaskState curr_state)
  {
    switch (curr_state)
    {
    case eRunning:
      return "Running  ";

    case eBlocked:
      return "Blocked  ";

    case eDeleted:
      return "Deleted  ";

    case eSuspended:
      return "Suspended";

    case eReady:
      return "Ready    ";

    default:
      return "UMM..help";
    }
  }

  task_error list_user_tasks()
  {

    Serial.print(ESP.getFreeHeap());
    Serial.print("/");
    Serial.print("400000");

    Serial.println("");
    //---------TASK LIST PRINT--------------
    const char *_spacer = "      ";
    Serial.println("ID          NAME    STATE    PRIORITY    MEM_USAGE");
    for (int i = 0; i < MAX_USER_TASKS; i++)
    {
      // Loop through the task list, if a task is in use print its info
      if (tasks[i].handle_in_use)
      {
        Serial.print(i); // ID
        Serial.print(_spacer);
        Serial.print(pcTaskGetTaskName(tasks[i].handle)); // Pretty name
        Serial.print(_spacer);
        Serial.print(state_to_name(eTaskGetState(tasks[i].handle))); // Task state
        Serial.print(_spacer);
        Serial.print(uxTaskPriorityGet(tasks[i].handle)); // Task prioraty
        Serial.print(_spacer);
        Serial.print(uxTaskGetStackHighWaterMark(tasks[i].handle)); // task current max mem usage
        Serial.println("");
      }
    }
    return noError;
  }

}
//---------------------------------------------------------------- Dummie task
void dummie_task(void *pvPerams)
{
  for (;;)
  {
    vTaskDelay(pdMS_TO_TICKS(10000));
    Serial.print(".");
  }
}

//---------------------------------------------------------- CLI stuff
namespace cli_handler
{

  typedef CommandParser<> MyCommandParser;
  MyCommandParser parser;

  void cmd_spawn(MyCommandParser::Argument *args, char *response)
  {
    Serial.print("Arg got: ");
    Serial.println(args[0].asString);
  }

  void cmd_rt_info(MyCommandParser::Argument *args, char *response)
  {
    task_handler::list_user_tasks();
  }

  void cmd_task_kill(MyCommandParser::Argument *args, char *response)
  {
    Serial.println(args[0].asInt64);
  }

  void register_commands()
  {
    parser.registerCommand("spawn", "s", &cmd_spawn);
    parser.registerCommand("top", "", &cmd_rt_info);
    parser.registerCommand("kill", "i", &cmd_task_kill);
  }

  void cli_handler(void *pvPerams)
  {
    bool cmd_ready = false;
    int buff_pointer = 0;
    const int cmd_buff_size = 128;
    char input_buffer[cmd_buff_size];

    // Put cute spartup message here
    Serial.println("Tiny FreeRTOS shell ready!");

    Serial.print(">");

    register_commands();

    for (;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10));

      while (Serial.available() && !cmd_ready)
      {
        input_buffer[buff_pointer] = Serial.read();

        if (input_buffer[buff_pointer] == '\n')
        {
          cmd_ready = true;
          Serial.println("");
        }
        else if ((input_buffer[buff_pointer] == '\b'))
        {
          if (buff_pointer == 0)
          {
            break;
          }
          Serial.print('\b');
          Serial.print(" ");
          Serial.print('\b');
          buff_pointer--;
        }
        else
        {
          Serial.write(input_buffer[buff_pointer]);
          buff_pointer++;
          if (buff_pointer >= cmd_buff_size)
          {
            buff_pointer = cmd_buff_size - 1;
          }
        }
      }

      if (cmd_ready)
      {
        input_buffer[buff_pointer - 1] = '\0';
        char response[MyCommandParser::MAX_RESPONSE_SIZE];
        parser.processCommand(input_buffer, response);

        cmd_ready = false;
        buff_pointer = 0;
        Serial.println(response);
        Serial.print(">");
      }
    }
  }
}

void setup()
{
  Serial.begin(115200);
  task_handler::spawn_new(cli_handler::cli_handler, 10, 10024, "USER SHELL");
  Serial.println("Task starter leaving, have fun!");
  vTaskDelete(NULL); // Kill the arduino task
}

void loop(){}; // Will never execute
