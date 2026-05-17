/** This is a block doc comment. */
void main()
{
    /* regular block comment - ignored */
    int x;
    // regular line comment - ignored
    /// This is a line doc comment.
    x = 10;
    /** Multi-line doc:
     *  describes function purpose
     *  spans several lines */
    x = 20;
    /**/
    x = 30;
    /// Final doc note
}
