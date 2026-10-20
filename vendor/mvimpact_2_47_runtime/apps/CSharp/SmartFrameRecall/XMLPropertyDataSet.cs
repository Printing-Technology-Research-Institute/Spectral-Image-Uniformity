namespace mv.impact.acquire.examples
{
    public class XMLPropertyDataSet
    {
        private string className_;
        private string name_;
        private string value_;
        private PropertyDataType dataType_;

        public enum PropertyDataType
        {
            pdtInteger = 0,
            pdtDouble = 1,
            pdtBool = 2
        };

        public XMLPropertyDataSet(string className, string name, string value, PropertyDataType dataType)
        {
            this.className_ = className;
            this.name_ = name;
            this.value_ = value;
            this.dataType_ = dataType;
        }

        public string ClassName { get { return className_; } }
        public string Name { get { return name_; } }
        public string Value { get { return value_; } }
        public PropertyDataType DataType { get { return dataType_; } }
    }
}