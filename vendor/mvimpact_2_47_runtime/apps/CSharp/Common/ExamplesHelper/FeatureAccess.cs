using System;
using System.Collections.Generic;

namespace mv.impact.acquire.examples.helper
{
    public static class FeatureAccess
    {
        #region public methods ------------------------------------------------

        //-----------------------------------------------------------------------------
        public static void displayPropertyData(Property prop)
        //-----------------------------------------------------------------------------
        {
            string name = prop.name;
            Console.WriteLine("Property '{0}' currently specifies the following flags: {1}", name, prop.flagsAsString());
            if (prop.hasMinValue)
            {
                Console.WriteLine("The minimum value of '{0}' is {1}", name, prop.readS(TPropertyLimits.plMinValue));
            }
            if (prop.hasMaxValue)
            {
                Console.WriteLine("The maximum value of '{0}' is {1}", name, prop.readS(TPropertyLimits.plMaxValue));
            }
            if (prop.hasStepWidth)
            {
                Console.WriteLine("The increment of '{0}' is {1}", name, prop.readS(TPropertyLimits.plStepWidth));
            }
            if (prop.hasDict)
            {
                Console.WriteLine("'{0}' defines a dictionary. Valid values are: ", name);
                TComponentType type = prop.type;
                if (type == TComponentType.ctPropInt)
                {
                    PropertyI p = new PropertyI(prop.hObj);
                    List<KeyValuePair<String, int>> sequence = new List<KeyValuePair<String, int>>();
                    p.getTranslationDict(sequence);
                    foreach (KeyValuePair<String, int> entry in sequence)
                    {
                        Console.WriteLine("[{0}]: {1}", entry.Value, entry.Key);
                    }
                }
                else if (type == TComponentType.ctPropInt64)
                {
                    PropertyI64 p = new PropertyI64(prop.hObj);
                    List<KeyValuePair<String, long>> sequence = new List<KeyValuePair<String, long>>();
                    p.getTranslationDict(sequence);
                    foreach (KeyValuePair<String, long> entry in sequence)
                    {
                        Console.WriteLine("[{0}]: {1}", entry.Value, entry.Key);
                    }
                }
                else if (type == TComponentType.ctPropFloat)
                {
                    PropertyF p = new PropertyF(prop.hObj);
                    List<KeyValuePair<String, double>> sequence = new List<KeyValuePair<String, double>>();
                    p.getTranslationDict(sequence);
                    foreach (KeyValuePair<String, double> entry in sequence)
                    {
                        Console.WriteLine("[{0}]: {1}", entry.Value, entry.Key);
                    }
                }
                else
                {
                    Console.WriteLine("Error! Unhandled enum prop type: {0}", prop.typeAsString);
                }
            }
            Console.WriteLine("The current value of '{0}' is: {1}", name, prop.readS());
        }

        //-----------------------------------------------------------------------------
        public static bool displayPropertyDataWithValidation(Property prop, string name)
        //-----------------------------------------------------------------------------
        {
            if (!prop.isValid)
            {
                Console.WriteLine("Property '{0}' is not supported/available.", name);
                return false;
            }
            displayPropertyData(prop);
            return true;
        }

        //-----------------------------------------------------------------------------
        public static void modifyPropertyValue(Property prop, string param, string index)
        //-----------------------------------------------------------------------------
        {
            try
            {
                string name = prop.name;
                int valIndex = 0;
                if (prop.isWriteable)
                {
                    if (param.Length == 0)
                    {
                        Console.Write("Enter the new value for '{0}': ", name);
                        string val = Console.ReadLine();
                        if (prop.valCount > 1)
                        {
                            Console.WriteLine("'{0}' defines {1} values. Enter the index (zero-based) of the value to modify: ", name, prop.valCount);
                            valIndex = Convert.ToInt32(Console.ReadLine());
                        }
                        prop.writeS(val, valIndex);
                    }
                    else
                    {
                        if (index.Length != 0)
                        {
                            valIndex = Convert.ToInt32(index);
                        }
                        prop.writeS(param, valIndex);
                    }
                }
                else
                {
                    Console.WriteLine("'{0}' is read-only, thus can't be modified.", name);
                }
            }
            catch (ImpactAcquireException e)
            {
                Console.WriteLine("An exception occurred: {0} (error code: {1})", e.errorString, e.errorCodeAsString);
            }
        }

        //-----------------------------------------------------------------------------
        public static void displayAndModifyPropertyDataWithValidation(Property prop, string name)
        //-----------------------------------------------------------------------------
        {
            if (displayPropertyDataWithValidation(prop, name))
            {
                modifyPropertyValue(prop, "", "");
            }
            Console.WriteLine();
        }

        //-----------------------------------------------------------------------------
        public static void conditionalSetProperty<T>(Property prop, T value, bool boSilent) where T : IConvertible
        //-----------------------------------------------------------------------------
        {
            if (prop.isValid)
            {
                switch (prop.type)
                {
                    case TComponentType.ctPropInt:
                        {
                            PropertyI p = new PropertyI(prop.hObj);
                            int intValue = Convert.ToInt32(value);
                            if ((p.hasDict && !p.listOfValidValues.Contains(intValue)) ||
                                ((p.hasMinValue && p.minValue > intValue) || (p.hasMaxValue && p.maxValue < intValue)))
                            {
                                return;
                            }
                            p.write(intValue);
                        }
                        break;
                    case TComponentType.ctPropInt64:
                        {
                            PropertyI64 p = new PropertyI64(prop.hObj);
                            long longValue = Convert.ToInt64(value);
                            if ((p.hasDict && !p.listOfValidValues.Contains(longValue)) ||
                                ((p.hasMinValue && p.minValue > longValue) || (p.hasMaxValue && p.maxValue < longValue)))
                            {
                                return;
                            }
                            p.write(longValue);
                        }
                        break;
                    case TComponentType.ctPropFloat:
                        {
                            PropertyF p = new PropertyF(prop.hObj);
                            double doubleValue = Convert.ToDouble(value);
                            if ((p.hasDict && !p.listOfValidValues.Contains(doubleValue)) ||
                                ((p.hasMinValue && p.minValue > doubleValue) || (p.hasMaxValue && p.maxValue < doubleValue)))
                            {
                                return;
                            }
                            p.write(doubleValue);
                        }
                        break;
                }

                if (!boSilent)
                {
                    Console.WriteLine("Property '{0}' set to '{1}'.", prop.name, prop.readS());
                }
            }
        }

        public static void conditionalSetProperty<T>(Property prop, T value) where T : IConvertible
        {
            conditionalSetProperty<T>(prop, value, false);
        }

        #endregion
    }
}