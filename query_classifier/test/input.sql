select sleep(2);
select * from tst where lname like '%e%' order by fname;
insert into tst values ("Jane","Doe"),("Daisy","Duck"),("Marie","Curie");
update tst set fname="Farmer", lname="McDonald" where lname="%Doe" and fname="John";
create table tmp as select * from t1;
create temporary table tmp as select * from t1;
select @@server_id;
select @OLD_SQL_NOTES;
SET autocommit=1;
SET autocommit=0;
BEGIN;
ROLLBACK;
COMMIT;
use X;
select last_insert_id();
select @@last_insert_id;
select @@identity;
select if(@@hostname='box02','prod_mariadb02','n');
select next value for seq1;
select nextval(seq1);
select seq1.nextval;
SELECT GET_LOCK('lock1',10);
SELECT IS_FREE_LOCK('lock1');
SELECT IS_USED_LOCK('lock1');
SELECT RELEASE_LOCK('lock1');
deallocate prepare select_stmt;
SELECT a FROM tbl FOR UPDATE;
SELECT a INTO OUTFILE 'out.txt';
SELECT a INTO DUMPFILE 'dump.txt';
SELECT a INTO @var;
select timediff(cast('2004-12-30 12:00:00' as time), '12:00:00');
(select 1 as a from t1) union all (select 1 from dual) limit 1;
SET @saved_cs_client= @@character_set_client;
SELECT 1 AS c1 FROM t1 ORDER BY ( SELECT 1 AS c2 FROM t1 GROUP BY GREATEST(LAST_INSERT_ID(), t1.a) ORDER BY GREATEST(LAST_INSERT_ID(), t1.a) LIMIT 1);
SET PASSWORD FOR 'user'@'10.0.0.1'='*C50EB75D7CB4C76B5264218B92BC69E6815B057A';
SELECT UTC_TIMESTAMP();
SELECT COUNT(IF(!c.ispackage, 1, NULL)) as cnt FROM test FOR UPDATE;
SELECT handler FROM abc FOR UPDATE;
SELECT * FROM test LOCK IN SHARE MODE;
SELECT * FROM test FOR SHARE;
DELETE x FROM x JOIN (SELECT id FROM y) y ON x.id = y.id;
SELECT @@identity;
SELECT @@last_gtid;
SELECT @@last_insert_id;
SET autocommit=1,names UTF8MB4;
SET names UTF8MB4,autocommit=1;
