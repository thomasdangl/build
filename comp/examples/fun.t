
gen = ()
{
	-> "World";
}

run = (str)
{
	sprintf(str, "Hello %s!\n", gen());
	-> 0;
}

str = calloc(1, 255);
run(str);
printf(str);
free(str);

