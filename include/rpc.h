#define BUF_SIZE 256 //worst case float32 * 3 + float32 * 3 + int32 + int32  

typedef enum Command {
  read_sensor, // arg1: encoder id, ret1: ticks, ret2: validity
  run_motor, // arg1: pwm freq, ret: validity
  pid_to_position, // arg1: x, arg2: y, arg3: z, ret1: error_in_pos, ret2: validity
  finger_pos, //TBD not designing for this. Unlikely that we need it, but parallizability might make it useful 
} Command;


typedef struct Args {
  float arg1;
  float arg2;
  float arg3;
} Args;

typedef struct Ret { 
  float arg1;
  float arg2;
  float arg3;
  int err;
} Ret;

typedef struct Call { 
  Command function_enum;
  Args* args;
  Ret* ret;
} Call;

// Call Encoder(); This is a macro. It needs to adapt to different types. I should just be able to call it and it'll be able to in place make a Call object 
void Serialize(const Call* call, char* buffer); //set ret to be empty? So that you can pass it, then recieve it next.
void Deserialize(const char* buffer, Call* call); // parse the function_enum, args, etc into a call 
void Dispatcher(Call* call); // Call functions and deal with typecasting 

void PrintSerialized(const char* buffer);






