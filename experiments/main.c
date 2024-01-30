
#include <stdbool.h>
#include "cronet_c.h"



int main(int argc, const char* argv[]) 
{
    Cronet_EnginePtr cronet_engine = Cronet_Engine_Create();
    Cronet_EngineParamsPtr engine_params = Cronet_EngineParams_Create();
    Cronet_EngineParams_user_agent_set(engine_params, "CronetSample/1");
    Cronet_EngineParams_enable_quic_set(engine_params, true);

    Cronet_Engine_StartWithParams(cronet_engine, engine_params);
    Cronet_EngineParams_Destroy(engine_params);
 
    Cronet_Engine_Shutdown(cronet_engine);
    Cronet_Engine_Destroy(cronet_engine);
    return 0;
}
