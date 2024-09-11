namespace YACC.Data
{
    internal class TargetDef
    {
        public List<StepDef> Steps;

        public TargetDef(Dictionary<object, object> data)
        {
            Steps = new List<StepDef>();

            if (data.ContainsKey("steps"))
            {
                var stepsList = (List<object>)data["steps"];

                foreach (var step in stepsList)
                {
                    var stepData = (Dictionary<object, object>)step;

                    var stepDef = new StepDef(stepData);

                    Steps.Add(stepDef);
                }
            }
        }
    }
}
